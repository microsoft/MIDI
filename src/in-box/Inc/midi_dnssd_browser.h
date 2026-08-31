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
#include <map>
#include <mutex>
#include <string>
#include <vector>

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
    // Callbacks are raised on a DNS thread pool thread, never with the internal lock held.
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

        // serviceType is the full query name, for example "_midi2._udp.local".
        HRESULT Start(
            _In_ std::wstring const& serviceType,
            _In_ ServiceHandler onAdded,
            _In_ UpdatedHandler onUpdated,
            _In_ RemovedHandler onRemoved)
        {
            auto lock = std::unique_lock{ m_lock };

            if (m_running) return S_FALSE;

            m_serviceType = serviceType;
            m_onAdded = std::move(onAdded);
            m_onUpdated = std::move(onUpdated);
            m_onRemoved = std::move(onRemoved);

            m_services.clear();
            m_hostIPv4.clear();
            m_hostIPv6.clear();

            SplitServiceType(serviceType, m_typeLabels, m_domain);

            m_cancel = DNS_SERVICE_CANCEL{ };

            DNS_SERVICE_BROWSE_REQUEST request{ };
            request.Version = DNS_QUERY_REQUEST_VERSION1;
            request.InterfaceIndex = 0;
            request.QueryName = m_serviceType.c_str();
            request.pBrowseCallback = &MidiDnssdBrowser::BrowseCallback;
            request.pQueryContext = this;

            m_running = true;

            lock.unlock();

            auto const status = DnsServiceBrowse(&request, &m_cancel);

            if (status != DNS_REQUEST_PENDING)
            {
                auto relock = std::unique_lock{ m_lock };
                m_running = false;

                return HRESULT_FROM_WIN32(status);
            }

            return S_OK;
        }

        // DnsServiceBrowseCancel is the documented way to end the operation, and no further
        // callbacks are delivered once it returns. The stopped flag guards a callback which was
        // already inside the trampoline when cancellation began.
        void Stop()
        {
            {
                auto lock = std::unique_lock{ m_lock };

                if (!m_running) return;

                m_running = false;
            }

            DnsServiceBrowseCancel(&m_cancel);

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
        };

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
            auto browser = reinterpret_cast<MidiDnssdBrowser*>(context);

            if (browser == nullptr)
            {
                if (records != nullptr) DnsRecordListFree(records, DnsFreeRecordList);

                return;
            }

            browser->m_callbacksInFlight++;

            if (records != nullptr)
            {
                browser->HandleRecords(status, records);

                DnsRecordListFree(records, DnsFreeRecordList);
            }

            browser->m_callbacksInFlight--;
        }

        void HandleRecords(_In_ DWORD /*status*/, _In_ PDNS_RECORD records)
        {
            std::vector<MidiDnssdService> added;
            std::vector<std::pair<MidiDnssdService, uint32_t>> updated;
            std::vector<std::pair<std::wstring, std::wstring>> removed;

            {
                auto lock = std::unique_lock{ m_lock };

                if (!m_running) return;

                // Addresses first: an A record in the same batch as the SRV that needs it must
                // not be missed just because it appears later in the list.
                for (auto record = records; record != nullptr; record = record->pNext)
                {
                    if (record->wType == DNS_TYPE_A || record->wType == DNS_TYPE_AAAA)
                    {
                        ApplyAddressRecord(record);
                    }
                }

                for (auto record = records; record != nullptr; record = record->pNext)
                {
                    switch (record->wType)
                    {
                    case DNS_TYPE_PTR: ApplyPtrRecord(record, removed); break;
                    case DNS_TYPE_SRV: ApplySrvRecord(record, removed); break;
                    case DNS_TYPE_TEXT: ApplyTextRecord(record); break;
                    default: break;
                    }
                }

                RefreshAddressesAndCollectChanges(added, updated);
            }

            // Outside the lock: a handler is free to call back into this object.
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

        void ApplyAddressRecord(_In_ PDNS_RECORD record)
        {
            auto const host = SafeName(record->pName);

            if (host.empty()) return;

            auto const key = ToLower(host);

            if (record->wType == DNS_TYPE_A)
            {
                if (record->dwTtl == 0)
                {
                    m_hostIPv4.erase(key);

                    return;
                }

                AddUnique(m_hostIPv4[key], FormatIPv4(record->Data.A.IpAddress));
            }
            else
            {
                if (record->dwTtl == 0)
                {
                    m_hostIPv6.erase(key);

                    return;
                }

                AddUnique(m_hostIPv6[key], FormatIPv6(record->Data.AAAA.Ip6Address));
            }
        }

        void ApplyPtrRecord(
            _In_ PDNS_RECORD record,
            _Inout_ std::vector<std::pair<std::wstring, std::wstring>>& removed)
        {
            auto const owner = SafeName(record->pName);

            // Only the PTR for the type we asked about names an instance of it.
            if (_wcsicmp(owner.c_str(), m_serviceType.c_str()) != 0) return;

            auto const fullName = SafeName(record->Data.PTR.pNameHost);

            if (fullName.empty()) return;

            if (record->dwTtl == 0)
            {
                RemoveService(fullName, removed);

                return;
            }

            auto& tracked = EnsureService(fullName);

            tracked.Service.LastSeenTickCount = GetTickCount64();
        }

        void ApplySrvRecord(
            _In_ PDNS_RECORD record,
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

        void RefreshAddressesAndCollectChanges(
            _Inout_ std::vector<MidiDnssdService>& added,
            _Inout_ std::vector<std::pair<MidiDnssdService, uint32_t>>& updated)
        {
            for (auto& entry : m_services)
            {
                auto& tracked = entry.second;

                if (!tracked.Service.IsResolved()) continue;

                auto const hostKey = ToLower(tracked.Service.HostName);

                auto const v4 = m_hostIPv4.find(hostKey);
                auto const v6 = m_hostIPv6.find(hostKey);

                auto previous = tracked.Service;

                tracked.Service.IPv4Addresses = (v4 == m_hostIPv4.end()) ? std::vector<std::wstring>{ } : v4->second;
                tracked.Service.IPv6Addresses = (v6 == m_hostIPv6.end()) ? std::vector<std::wstring>{ } : v6->second;

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

        DNS_SERVICE_CANCEL m_cancel{ };

        std::wstring m_serviceType;
        std::wstring m_typeLabels;
        std::wstring m_domain;

        std::map<std::wstring, TrackedService> m_services;
        std::map<std::wstring, std::vector<std::wstring>> m_hostIPv4;
        std::map<std::wstring, std::vector<std::wstring>> m_hostIPv6;

        // Returned when the tracked service cap is hit, so callers always get a valid reference.
        TrackedService m_overflow{ };

        ServiceHandler m_onAdded;
        UpdatedHandler m_onUpdated;
        RemovedHandler m_onRemoved;
    };
}
