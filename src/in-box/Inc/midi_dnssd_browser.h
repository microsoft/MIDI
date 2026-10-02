// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Shared DNS-SD service browser, used by both the Network MIDI 2.0 service transport and the
// Windows.Devices.Midi2 SDK.
//
// This exists because neither WinRT discovery API reports a service going away.
// Windows.Networking.ServiceDiscovery.Dnssd.DnssdServiceWatcher has only an Added event, by
// design, and its documentation redirects to Windows.Devices.Enumeration, whose DNS-SD
// DeviceWatcher never raises Removed either: measured over one host's life it produced 1 Added,
// 143 Updated and 0 Removed, despite the departing host multicasting a correct TTL 0 goodbye.
// Neither surfaces System.Devices.Dnssd.Ttl, and there is no presence property to filter on.
// See https://github.com/microsoft/MIDI/issues/1149 and /issues/1003.
//
// DnsServiceBrowse does deliver the goodbye, within about two milliseconds, along with the A,
// AAAA, PTR, SRV and TXT records needed to describe the service without a second resolve step.
//
// There is one browse per network adapter, because a record does not say which adapter it came
// in on and a browse limited to one adapter does. Every adapter has a link-local IPv6 address in
// the same fe80:: range, so one from a device is only usable with a %scope naming the adapter it
// was seen on. Keeping each adapter's answers apart also means a device on two networks, or one
// whose address changed, is described by what each network says about it now rather than by
// everything it has ever said. Measured on Windows 11: a browse limited to one adapter reports
// only the devices on that adapter, and this PC's own hosts with that adapter's addresses only.
//
// This parses data from the network. Every field is treated as untrusted and length limited.
// ============================================================================

#pragma once

#include <windows.h>
#include <windns.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "midi_network_adapters.h"

#pragma comment(lib, "Dnsapi.lib")

namespace WindowsMidiServicesInternal
{
    // A remote can advertise arbitrary text of arbitrary length. These caps keep a hostile or
    // broken responder from growing our maps without bound. They are deliberately far larger
    // than anything the MIDI 2.0 Network specification permits.
    constexpr size_t MidiDnssdMaxNameLength = 512;
    constexpr size_t MidiDnssdMaxTextValueLength = 1024;
    constexpr size_t MidiDnssdMaxTextAttributes = 64;
    constexpr size_t MidiDnssdMaxAddressesPerHost = 32;
    constexpr size_t MidiDnssdMaxTrackedServices = 512;
    constexpr size_t MidiDnssdMaxTrackedHosts = 512;

    // An adapter coming up changes several addresses in a row. Browsing starts once they settle.
    constexpr uint32_t MidiDnssdAdapterSettleMilliseconds = 1500;


    // Which parts of an advertisement changed. Mirrors the SDK's
    // MidiNetworkAdvertisedHostChangedProperties, so the two never drift.
    enum MidiDnssdServiceChangedFields : uint32_t
    {
        MidiDnssdChangedNone = 0x00000000,
        MidiDnssdChangedHostName = 0x00000001,
        MidiDnssdChangedPort = 0x00000002,
        MidiDnssdChangedIPv4Addresses = 0x00000004,
        MidiDnssdChangedIPv6Addresses = 0x00000008,
        MidiDnssdChangedTextAttributes = 0x00000010,
    };


    struct MidiDnssdService
    {
        std::wstring FullName;              // "bomebox-8q6d2z-1._midi2._udp.local"
        std::wstring ServiceInstanceName;   // "bomebox-8q6d2z-1"
        std::wstring ServiceType;           // "_midi2._udp"
        std::wstring Domain;                // "local"
        std::wstring HostName;              // "bomebox.local"
        uint16_t Port{ 0 };

        std::vector<std::wstring> IPv4Addresses;

        // A link-local address ends in the %scope of the adapter it was seen on
        std::vector<std::wstring> IPv6Addresses;

        std::map<std::wstring, std::wstring> TextAttributes;

        uint64_t LastSeenTickCount{ 0 };

        std::wstring TextAttribute(_In_ std::wstring const& key) const
        {
            auto const it = TextAttributes.find(key);

            return it == TextAttributes.end() ? std::wstring{ } : it->second;
        }

        std::wstring UmpEndpointName() const { return TextAttribute(L"UMPEndpointName"); }
        std::wstring ProductInstanceId() const { return TextAttribute(L"ProductInstanceId"); }

        // The identifier Windows.Devices.Enumeration used for the same service. Reproduced
        // exactly because it is what existing configuration files store as the client match id.
        std::wstring DeviceId() const { return L"DnsSd#" + FullName + L"#0"; }

        bool IsResolved() const { return !HostName.empty() && Port != 0; }

        uint32_t ChangedFieldsSince(_In_ MidiDnssdService const& previous) const
        {
            uint32_t changed{ MidiDnssdChangedNone };

            if (HostName != previous.HostName)             changed |= MidiDnssdChangedHostName;
            if (Port != previous.Port)                     changed |= MidiDnssdChangedPort;
            if (IPv4Addresses != previous.IPv4Addresses)   changed |= MidiDnssdChangedIPv4Addresses;
            if (IPv6Addresses != previous.IPv6Addresses)   changed |= MidiDnssdChangedIPv6Addresses;
            if (TextAttributes != previous.TextAttributes) changed |= MidiDnssdChangedTextAttributes;

            return changed;
        }
    };


    // Browses one DNS-SD service type and reports instances arriving, changing, and going away.
    // Callbacks are raised on a DNS or thread pool thread, never with the internal lock held, and
    // a handler must not call Stop.
    class MidiDnssdBrowser
    {
    public:
        using ServiceHandler = std::function<void(MidiDnssdService const&)>;
        using UpdatedHandler = std::function<void(MidiDnssdService const&, uint32_t changedFields)>;
        using RemovedHandler = std::function<void(std::wstring const& fullName, std::wstring const& deviceId)>;

        MidiDnssdBrowser() = default;
        ~MidiDnssdBrowser() { Stop(); }

        MidiDnssdBrowser(MidiDnssdBrowser const&) = delete;
        MidiDnssdBrowser& operator=(MidiDnssdBrowser const&) = delete;

        // serviceType is the full query name, for example "_midi2._udp.local". Succeeds with no
        // adapter up, and starts browsing on each adapter as it comes up.
        HRESULT Start(
            _In_ std::wstring const& serviceType,
            _In_ ServiceHandler onAdded,
            _In_ UpdatedHandler onUpdated,
            _In_ RemovedHandler onRemoved)
        {
            {
                auto lock = std::unique_lock{ m_lock };

                if (m_running) return S_FALSE;

                m_serviceType = serviceType;
                m_onAdded = std::move(onAdded);
                m_onUpdated = std::move(onUpdated);
                m_onRemoved = std::move(onRemoved);

                m_services.clear();
                m_hostAddresses.clear();

                SplitServiceType(serviceType, m_typeLabels, m_domain);

                m_running = true;
            }

            // Without the notification an adapter which came up later would never be browsed,
            // so this falls back to the single browse across every adapter it used to make.
            auto const perAdapter = m_networkMonitor.Start([this]() { (void)SyncAdapters(); }, MidiDnssdAdapterSettleMilliseconds);

            auto const hr = perAdapter ? SyncAdapters() : StartBrowse(0, UINT32_MAX);

            if (FAILED(hr))
            {
                Stop();

                return hr;
            }

            return S_OK;
        }

        // DnsServiceBrowseCancel is the documented way to end the operation, and no further
        // callbacks are delivered once it returns. The inactive flag guards a callback which was
        // already inside the trampoline when cancellation began.
        void Stop()
        {
            {
                auto lock = std::unique_lock{ m_lock };

                if (!m_running) return;

                m_running = false;
            }

            // Waits out an adapter change being handled, so nothing starts a browse after this
            m_networkMonitor.Stop();

            std::vector<AdapterBrowse*> active;

            {
                auto lock = std::unique_lock{ m_lock };

                for (auto& entry : m_browses)
                {
                    if (entry.second->Active)
                    {
                        entry.second->Active = false;
                        active.push_back(entry.second.get());
                    }
                }
            }

            for (auto const browse : active)
            {
                DnsServiceBrowseCancel(&browse->Cancel);
            }

            // Lets any callback which had already entered finish before handlers are released.
            while (m_callbacksInFlight.load() > 0)
            {
                Sleep(1);
            }

            auto lock = std::unique_lock{ m_lock };

            m_onAdded = nullptr;
            m_onUpdated = nullptr;
            m_onRemoved = nullptr;
        }

        bool IsRunning() const
        {
            auto lock = std::unique_lock{ m_lock };

            return m_running;
        }

        std::vector<MidiDnssdService> EnumeratedServices() const
        {
            auto lock = std::unique_lock{ m_lock };

            std::vector<MidiDnssdService> result;
            result.reserve(m_services.size());

            for (auto const& entry : m_services)
            {
                if (entry.second.Reported)
                {
                    result.push_back(entry.second.Service);
                }
            }

            return result;
        }

        bool TryGetService(_In_ std::wstring const& fullName, _Out_ MidiDnssdService& service) const
        {
            auto lock = std::unique_lock{ m_lock };

            auto const it = m_services.find(ToLower(fullName));

            if (it == m_services.end() || !it->second.Reported) return false;

            service = it->second.Service;

            return true;
        }


    private:
        struct TrackedService
        {
            MidiDnssdService Service;
            bool Reported{ false };

            // the adapters its PTR or SRV record arrived on
            std::set<uint32_t> Adapters;
        };

        // What one adapter last said about one device's addresses
        struct AdapterAddresses
        {
            std::vector<std::wstring> IPv4;
            std::vector<std::wstring> IPv6;
        };

        // One adapter's browse, and the context its DNS client callbacks carry. Interface index 0
        // is the single browse across every adapter, used when adapters cannot be followed.
        struct AdapterBrowse
        {
            MidiDnssdBrowser* Browser{ nullptr };
            uint32_t InterfaceIndex{ 0 };
            uint32_t Metric{ UINT32_MAX };
            DNS_SERVICE_CANCEL Cancel{ };

            // Only changed under the lock. Cleared before the browse is canceled, so a callback
            // already on its way is ignored.
            bool Active{ false };
        };

        // One adapter's browse, or every adapter's for index zero
        HRESULT StartBrowse(_In_ uint32_t const interfaceIndex, _In_ uint32_t const metric)
        {
            AdapterBrowse* browse{ nullptr };

            {
                auto lock = std::unique_lock{ m_lock };

                if (!m_running) return S_FALSE;

                auto& entry = m_browses[interfaceIndex];

                if (entry == nullptr)
                {
                    entry = std::make_unique<AdapterBrowse>();
                    entry->Browser = this;
                    entry->InterfaceIndex = interfaceIndex;
                }

                entry->Metric = metric;

                if (entry->Active) return S_OK;

                entry->Active = true;
                entry->Cancel = DNS_SERVICE_CANCEL{ };

                browse = entry.get();
            }

            DNS_SERVICE_BROWSE_REQUEST request{ };
            request.Version = DNS_QUERY_REQUEST_VERSION1;
            request.InterfaceIndex = interfaceIndex;
            request.QueryName = m_serviceType.c_str();
            request.pBrowseCallback = &MidiDnssdBrowser::BrowseCallback;
            request.pQueryContext = browse;

            auto const status = DnsServiceBrowse(&request, &browse->Cancel);

            if (status != DNS_REQUEST_PENDING)
            {
                auto lock = std::unique_lock{ m_lock };
                browse->Active = false;

                return HRESULT_FROM_WIN32(status);
            }

            return S_OK;
        }

        // Browses each adapter which is up and carries multicast, and stops browsing each one
        // which went away. Runs at start and after every settled adapter change.
        HRESULT SyncAdapters()
        {
            // the thread pool could otherwise run two of these at once
            auto syncLock = std::unique_lock{ m_syncLock };

            // interface index and metric
            std::map<uint32_t, uint32_t> wanted;

            for (auto const& adapter : GetMidiNetworkAdapters())
            {
                if (!adapter.IsUsable() || !adapter.SupportsMulticast || adapter.InterfaceIndex() == 0) continue;

                wanted[adapter.InterfaceIndex()] = adapter.Metric;
            }

            std::vector<AdapterBrowse*> gone;

            {
                auto lock = std::unique_lock{ m_lock };

                if (!m_running) return S_FALSE;

                for (auto& entry : m_browses)
                {
                    if (entry.second->Active && wanted.find(entry.first) == wanted.end())
                    {
                        entry.second->Active = false;
                        gone.push_back(entry.second.get());
                    }
                }
            }

            for (auto const browse : gone)
            {
                DnsServiceBrowseCancel(&browse->Cancel);
            }

            HRESULT lastFailure{ S_OK };
            bool anyStarted{ false };

            for (auto const& entry : wanted)
            {
                auto const hr = StartBrowse(entry.first, entry.second);

                if (SUCCEEDED(hr)) anyStarted = true;
                else lastFailure = hr;
            }

            if (!gone.empty())
            {
                ForgetAdapters(gone);
            }

            // No adapter being up is not a failure. Browsing starts with the next change.
            return (anyStarted || wanted.empty()) ? S_OK : lastFailure;
        }

        // What an adapter which went away said is no longer true. A service seen only there is
        // reported gone, and one also seen on another adapter loses that adapter's addresses.
        void ForgetAdapters(_In_ std::vector<AdapterBrowse*> const& gone)
        {
            std::vector<MidiDnssdService> added;
            std::vector<std::pair<MidiDnssdService, uint32_t>> updated;
            std::vector<std::pair<std::wstring, std::wstring>> removed;

            {
                auto lock = std::unique_lock{ m_lock };

                if (!m_running) return;

                for (auto const browse : gone)
                {
                    auto const index = browse->InterfaceIndex;

                    for (auto host = m_hostAddresses.begin(); host != m_hostAddresses.end(); )
                    {
                        host->second.erase(index);
                        host = host->second.empty() ? m_hostAddresses.erase(host) : std::next(host);
                    }

                    std::vector<std::wstring> seenThere;

                    for (auto const& entry : m_services)
                    {
                        if (entry.second.Adapters.count(index) != 0) seenThere.push_back(entry.second.Service.FullName);
                    }

                    for (auto const& fullName : seenThere)
                    {
                        RemoveServiceFromAdapter(fullName, index, removed);
                    }
                }

                RefreshAddressesAndCollectChanges(added, updated);
            }

            RaiseEvents(added, updated, removed);
        }

        // Outside the lock: a handler is free to call back into this object.
        void RaiseEvents(
            _In_ std::vector<MidiDnssdService> const& added,
            _In_ std::vector<std::pair<MidiDnssdService, uint32_t>> const& updated,
            _In_ std::vector<std::pair<std::wstring, std::wstring>> const& removed)
        {
            for (auto const& service : added)
            {
                if (m_onAdded) m_onAdded(service);
            }

            for (auto const& entry : updated)
            {
                if (m_onUpdated) m_onUpdated(entry.first, entry.second);
            }

            for (auto const& entry : removed)
            {
                if (m_onRemoved) m_onRemoved(entry.first, entry.second);
            }
        }

        static std::wstring ToLower(_In_ std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });

            return value;
        }

        static std::wstring SafeName(_In_opt_ PCWSTR value)
        {
            if (value == nullptr) return { };

            std::wstring result{ value };

            if (result.length() > MidiDnssdMaxNameLength)
            {
                result.resize(MidiDnssdMaxNameLength);
            }

            // A trailing dot is legal in the wire form and would break suffix matching.
            while (!result.empty() && result.back() == L'.')
            {
                result.pop_back();
            }

            return result;
        }

        static void SplitServiceType(
            _In_ std::wstring const& serviceType,
            _Out_ std::wstring& typeLabels,
            _Out_ std::wstring& domain)
        {
            // "_midi2._udp.local" splits into "_midi2._udp" and "local"
            auto const lastDot = serviceType.find_last_of(L'.');

            if (lastDot == std::wstring::npos)
            {
                typeLabels = serviceType;
                domain.clear();

                return;
            }

            typeLabels = serviceType.substr(0, lastDot);
            domain = serviceType.substr(lastDot + 1);
        }

        static std::wstring FormatIPv4(_In_ DWORD const address)
        {
            auto const bytes = reinterpret_cast<uint8_t const*>(&address);

            wchar_t buffer[16]{ };
            swprintf_s(buffer, L"%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);

            return buffer;
        }

        // RFC 5952 presentation form, including the longest run of zero groups collapsed to "::".
        static std::wstring FormatIPv6(_In_ IP6_ADDRESS const& address)
        {
            uint16_t groups[8]{ };

            for (int i = 0; i < 8; i++)
            {
                groups[i] = static_cast<uint16_t>(
                    (static_cast<uint16_t>(address.IP6Byte[i * 2]) << 8) | address.IP6Byte[i * 2 + 1]);
            }

            int bestStart{ -1 };
            int bestLength{ 0 };
            int currentStart{ -1 };
            int currentLength{ 0 };

            for (int i = 0; i < 8; i++)
            {
                if (groups[i] == 0)
                {
                    if (currentStart < 0) currentStart = i;

                    currentLength++;

                    if (currentLength > bestLength)
                    {
                        bestStart = currentStart;
                        bestLength = currentLength;
                    }
                }
                else
                {
                    currentStart = -1;
                    currentLength = 0;
                }
            }

            // A single zero group is written out rather than compressed.
            if (bestLength < 2)
            {
                bestStart = -1;
                bestLength = 0;
            }

            std::wstring result;

            for (int i = 0; i < 8; )
            {
                if (i == bestStart)
                {
                    // Always both colons: the group appended next sees a trailing ':' and adds
                    // no separator of its own, so a single one here would lose a colon.
                    result += L"::";
                    i += bestLength;

                    continue;
                }

                if (!result.empty() && result.back() != L':')
                {
                    result += L':';
                }

                wchar_t buffer[8]{ };
                swprintf_s(buffer, L"%x", groups[i]);
                result += buffer;

                i++;
            }

            return result.empty() ? L"::" : result;
        }

        // fe80::/10 is on every adapter, so an address in it is only usable with its adapter named
        static std::wstring FormatIPv6(_In_ IP6_ADDRESS const& address, _In_ uint32_t const interfaceIndex)
        {
            auto text = FormatIPv6(address);

            if (interfaceIndex != 0 && address.IP6Byte[0] == 0xFE && (address.IP6Byte[1] & 0xC0) == 0x80)
            {
                text += L"%" + std::to_wstring(interfaceIndex);
            }

            return text;
        }

        static void AddUnique(_Inout_ std::vector<std::wstring>& list, _In_ std::wstring const& value)
        {
            if (value.empty() || list.size() >= MidiDnssdMaxAddressesPerHost) return;

            if (std::find(list.begin(), list.end(), value) == list.end())
            {
                list.push_back(value);
            }
        }

        static VOID WINAPI BrowseCallback(_In_ DWORD status, _In_ PVOID context, _In_ PDNS_RECORD records)
        {
            auto const browse = reinterpret_cast<AdapterBrowse*>(context);
            auto const browser = browse != nullptr ? browse->Browser : nullptr;

            if (browser == nullptr)
            {
                if (records != nullptr) DnsRecordListFree(records, DnsFreeRecordList);

                return;
            }

            browser->m_callbacksInFlight++;

            if (records != nullptr)
            {
                browser->HandleRecords(*browse, status, records);

                DnsRecordListFree(records, DnsFreeRecordList);
            }

            browser->m_callbacksInFlight--;
        }

        void HandleRecords(_In_ AdapterBrowse const& browse, _In_ DWORD /*status*/, _In_ PDNS_RECORD records)
        {
            std::vector<MidiDnssdService> added;
            std::vector<std::pair<MidiDnssdService, uint32_t>> updated;
            std::vector<std::pair<std::wstring, std::wstring>> removed;

            {
                auto lock = std::unique_lock{ m_lock };

                // stopped, or this adapter's browse was canceled while the callback was on its way
                if (!m_running || !browse.Active) return;

                auto const index = browse.InterfaceIndex;

                // An answer carries every address the device has on the adapter it went out on, so
                // the first one of each kind replaces what this adapter said before. Adding to it
                // instead kept every address a device had ever had.
                std::set<std::pair<std::wstring, WORD>> replaced;

                // Addresses first: an A record in the same batch as the SRV that needs it must
                // not be missed just because it appears later in the list.
                for (auto record = records; record != nullptr; record = record->pNext)
                {
                    if (record->wType == DNS_TYPE_A || record->wType == DNS_TYPE_AAAA)
                    {
                        ApplyAddressRecord(record, index, replaced);
                    }
                }

                for (auto record = records; record != nullptr; record = record->pNext)
                {
                    switch (record->wType)
                    {
                    case DNS_TYPE_PTR: ApplyPtrRecord(record, index, removed); break;
                    case DNS_TYPE_SRV: ApplySrvRecord(record, index, removed); break;
                    case DNS_TYPE_TEXT: ApplyTextRecord(record); break;
                    default: break;
                    }
                }

                RefreshAddressesAndCollectChanges(added, updated);
            }

            RaiseEvents(added, updated, removed);
        }

        void ApplyAddressRecord(
            _In_ PDNS_RECORD record,
            _In_ uint32_t const interfaceIndex,
            _Inout_ std::set<std::pair<std::wstring, WORD>>& replaced)
        {
            auto const host = SafeName(record->pName);

            if (host.empty()) return;

            auto const key = ToLower(host);
            bool const isIPv4 = record->wType == DNS_TYPE_A;

            auto const text = isIPv4 ?
                FormatIPv4(record->Data.A.IpAddress) :
                FormatIPv6(record->Data.AAAA.Ip6Address, interfaceIndex);

            auto hostEntry = m_hostAddresses.find(key);

            if (record->dwTtl == 0)
            {
                // A goodbye for this one address. Dropping every address the device has, as this
                // used to, left a device whose address changed with none at all.
                if (hostEntry == m_hostAddresses.end()) return;

                auto const adapterEntry = hostEntry->second.find(interfaceIndex);

                if (adapterEntry == hostEntry->second.end()) return;

                auto& list = isIPv4 ? adapterEntry->second.IPv4 : adapterEntry->second.IPv6;
                list.erase(std::remove(list.begin(), list.end(), text), list.end());

                return;
            }

            if (hostEntry == m_hostAddresses.end())
            {
                // A hostile responder could otherwise name unlimited hosts.
                if (m_hostAddresses.size() >= MidiDnssdMaxTrackedHosts) return;

                hostEntry = m_hostAddresses.emplace(key, std::map<uint32_t, AdapterAddresses>{ }).first;
            }

            auto& addresses = hostEntry->second[interfaceIndex];
            auto& list = isIPv4 ? addresses.IPv4 : addresses.IPv6;

            if (replaced.insert({ key, record->wType }).second)
            {
                list.clear();
            }

            AddUnique(list, text);
        }

        void ApplyPtrRecord(
            _In_ PDNS_RECORD record,
            _In_ uint32_t const interfaceIndex,
            _Inout_ std::vector<std::pair<std::wstring, std::wstring>>& removed)
        {
            auto const owner = SafeName(record->pName);

            // Only the PTR for the type we asked about names an instance of it.
            if (_wcsicmp(owner.c_str(), m_serviceType.c_str()) != 0) return;

            auto const fullName = SafeName(record->Data.PTR.pNameHost);

            if (fullName.empty()) return;

            // A goodbye on any adapter takes the service away on all of them. Waiting for one on
            // every adapter would leave it listed forever when a single goodbye is lost.
            if (record->dwTtl == 0)
            {
                RemoveService(fullName, removed);

                return;
            }

            auto& tracked = EnsureService(fullName);

            tracked.Adapters.insert(interfaceIndex);
            tracked.Service.LastSeenTickCount = GetTickCount64();
        }

        void ApplySrvRecord(
            _In_ PDNS_RECORD record,
            _In_ uint32_t const interfaceIndex,
            _Inout_ std::vector<std::pair<std::wstring, std::wstring>>& removed)
        {
            auto const fullName = SafeName(record->pName);

            if (fullName.empty() || !IsInstanceOfBrowsedType(fullName)) return;

            if (record->dwTtl == 0)
            {
                RemoveService(fullName, removed);

                return;
            }

            auto& tracked = EnsureService(fullName);

            tracked.Adapters.insert(interfaceIndex);
            tracked.Service.HostName = SafeName(record->Data.SRV.pNameTarget);
            tracked.Service.Port = record->Data.SRV.wPort;
            tracked.Service.LastSeenTickCount = GetTickCount64();
        }

        void ApplyTextRecord(_In_ PDNS_RECORD record)
        {
            auto const fullName = SafeName(record->pName);

            if (fullName.empty() || !IsInstanceOfBrowsedType(fullName)) return;

            // A TXT goodbye is handled by the matching PTR and SRV goodbyes; clearing the
            // attributes here would only make the record briefly look malformed.
            if (record->dwTtl == 0) return;

            auto& tracked = EnsureService(fullName);

            std::map<std::wstring, std::wstring> attributes;

            // Parenthesised because windows.h defines a min macro.
            auto const count = (std::min)(record->Data.TXT.dwStringCount, static_cast<DWORD>(MidiDnssdMaxTextAttributes));

            for (DWORD i = 0; i < count; i++)
            {
                auto const entry = record->Data.TXT.pStringArray[i];

                if (entry == nullptr) continue;

                std::wstring text{ entry };

                if (text.length() > MidiDnssdMaxTextValueLength)
                {
                    text.resize(MidiDnssdMaxTextValueLength);
                }

                auto const equals = text.find(L'=');

                if (equals == std::wstring::npos)
                {
                    attributes[text] = std::wstring{ };
                }
                else
                {
                    attributes[text.substr(0, equals)] = text.substr(equals + 1);
                }
            }

            tracked.Service.TextAttributes = std::move(attributes);
            tracked.Service.LastSeenTickCount = GetTickCount64();
        }

        bool IsInstanceOfBrowsedType(_In_ std::wstring const& fullName) const
        {
            if (fullName.length() <= m_serviceType.length() + 1) return false;

            auto const suffixStart = fullName.length() - m_serviceType.length();

            if (fullName[suffixStart - 1] != L'.') return false;

            return _wcsicmp(fullName.c_str() + suffixStart, m_serviceType.c_str()) == 0;
        }

        TrackedService& EnsureService(_In_ std::wstring const& fullName)
        {
            auto const key = ToLower(fullName);

            auto existing = m_services.find(key);

            if (existing != m_services.end())
            {
                return existing->second;
            }

            // A hostile responder could otherwise name unlimited instances.
            if (m_services.size() >= MidiDnssdMaxTrackedServices)
            {
                return m_overflow;
            }

            TrackedService tracked{ };

            tracked.Service.FullName = fullName;
            tracked.Service.ServiceType = m_typeLabels;
            tracked.Service.Domain = m_domain;

            if (fullName.length() > m_serviceType.length() + 1)
            {
                tracked.Service.ServiceInstanceName =
                    fullName.substr(0, fullName.length() - m_serviceType.length() - 1);
            }

            return m_services.emplace(key, std::move(tracked)).first->second;
        }

        void RemoveService(
            _In_ std::wstring const& fullName,
            _Inout_ std::vector<std::pair<std::wstring, std::wstring>>& removed)
        {
            auto const key = ToLower(fullName);

            auto const it = m_services.find(key);

            if (it == m_services.end()) return;

            if (it->second.Reported)
            {
                removed.emplace_back(it->second.Service.FullName, it->second.Service.DeviceId());
            }

            m_services.erase(it);
        }

        // Only for an adapter which went away. The service stays while another adapter has it.
        void RemoveServiceFromAdapter(
            _In_ std::wstring const& fullName,
            _In_ uint32_t const interfaceIndex,
            _Inout_ std::vector<std::pair<std::wstring, std::wstring>>& removed)
        {
            auto const it = m_services.find(ToLower(fullName));

            if (it == m_services.end()) return;

            it->second.Adapters.erase(interfaceIndex);

            if (!it->second.Adapters.empty()) return;

            RemoveService(fullName, removed);
        }

        void RefreshAddressesAndCollectChanges(
            _Inout_ std::vector<MidiDnssdService>& added,
            _Inout_ std::vector<std::pair<MidiDnssdService, uint32_t>>& updated)
        {
            for (auto& entry : m_services)
            {
                auto& tracked = entry.second;

                if (!tracked.Service.IsResolved()) continue;

                auto previous = tracked.Service;

                tracked.Service.IPv4Addresses.clear();
                tracked.Service.IPv6Addresses.clear();

                auto const host = m_hostAddresses.find(ToLower(tracked.Service.HostName));

                if (host != m_hostAddresses.end())
                {
                    // Metric, then interface index, so the adapter Windows would use comes first
                    std::vector<std::pair<uint32_t, uint32_t>> order;

                    for (auto const& adapter : host->second)
                    {
                        // only the adapters the service itself was seen on
                        if (!tracked.Adapters.empty() && tracked.Adapters.count(adapter.first) == 0) continue;

                        auto const browse = m_browses.find(adapter.first);

                        order.emplace_back(browse == m_browses.end() ? UINT32_MAX : browse->second->Metric, adapter.first);
                    }

                    std::sort(order.begin(), order.end());

                    for (auto const& item : order)
                    {
                        auto const& addresses = host->second.at(item.second);

                        for (auto const& address : addresses.IPv4) AddUnique(tracked.Service.IPv4Addresses, address);
                        for (auto const& address : addresses.IPv6) AddUnique(tracked.Service.IPv6Addresses, address);
                    }
                }

                if (!tracked.Reported)
                {
                    tracked.Reported = true;

                    added.push_back(tracked.Service);

                    continue;
                }

                auto const changed = tracked.Service.ChangedFieldsSince(previous);

                if (changed != MidiDnssdChangedNone)
                {
                    updated.emplace_back(tracked.Service, changed);
                }
            }
        }


        mutable std::mutex m_lock;
        bool m_running{ false };
        std::atomic<int> m_callbacksInFlight{ 0 };

        // Kept until the browser is destroyed, because each is the context a DNS client callback
        // carries. An adapter which comes back reuses its own.
        std::map<uint32_t, std::unique_ptr<AdapterBrowse>> m_browses;

        std::mutex m_syncLock;
        MidiNetworkChangeMonitor m_networkMonitor;

        std::wstring m_serviceType;
        std::wstring m_typeLabels;
        std::wstring m_domain;

        std::map<std::wstring, TrackedService> m_services;

        // Lower case host name, then interface index
        std::map<std::wstring, std::map<uint32_t, AdapterAddresses>> m_hostAddresses;

        // Returned when the tracked service cap is hit, so callers always get a valid reference.
        TrackedService m_overflow{ };

        ServiceHandler m_onAdded;
        UpdatedHandler m_onUpdated;
        RemovedHandler m_onRemoved;
    };
}
