// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// See midi_network_adapters.h. winsock2.h has to come before windows.h, which is why this lives
// in its own translation unit rather than in the header.
// ============================================================================

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <cwctype>
#include <vector>

#include "midi_network_adapters.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ole32.lib")

namespace WindowsMidiServicesInternal
{
    namespace
    {
        std::wstring FormatPhysicalAddress(_In_reads_(length) BYTE const* bytes, _In_ ULONG const length)
        {
            bool anyNonZero{ false };
            std::wstring text{};

            for (ULONG i = 0; i < length; i++)
            {
                wchar_t part[4]{};
                swprintf_s(part, L"%02X", bytes[i]);

                if (!text.empty()) text += L'-';
                text += part;

                anyNonZero = anyNonZero || bytes[i] != 0;
            }

            // all zeros identifies nothing, and would match every other adapter without one
            return anyNonZero ? text : std::wstring{};
        }

        std::wstring FormatAddress(_In_ SOCKADDR const* address)
        {
            wchar_t buffer[INET6_ADDRSTRLEN]{};

            if (address->sa_family == AF_INET)
            {
                auto const v4 = reinterpret_cast<SOCKADDR_IN const*>(address);
                if (InetNtopW(AF_INET, &v4->sin_addr, buffer, ARRAYSIZE(buffer)) == nullptr) return {};

                return buffer;
            }

            if (address->sa_family == AF_INET6)
            {
                auto const v6 = reinterpret_cast<SOCKADDR_IN6 const*>(address);
                if (InetNtopW(AF_INET6, &v6->sin6_addr, buffer, ARRAYSIZE(buffer)) == nullptr) return {};

                std::wstring text{ buffer };

                // every adapter has a link-local address, so one is only usable with its adapter named
                if (IN6_IS_ADDR_LINKLOCAL(&v6->sin6_addr) && v6->sin6_scope_id != 0)
                {
                    text += L"%" + std::to_wstring(v6->sin6_scope_id);
                }

                return text;
            }

            return {};
        }
    }

    std::vector<MidiNetworkAdapterInfo> GetMidiNetworkAdapters() noexcept
    {
        std::vector<MidiNetworkAdapterInfo> adapters{};

        try
        {
            ULONG const flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

            std::vector<uint8_t> buffer{};
            ULONG size = 16 * 1024;
            ULONG status = ERROR_BUFFER_OVERFLOW;

            // the list can grow between the call which sizes the buffer and the one which fills it
            for (int attempt = 0; attempt < 3 && status == ERROR_BUFFER_OVERFLOW; attempt++)
            {
                buffer.resize(size);
                status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
            }

            if (status != NO_ERROR) return adapters;

            for (auto adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES const*>(buffer.data()); adapter != nullptr; adapter = adapter->Next)
            {
                if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->IfType == IF_TYPE_TUNNEL) continue;

                MidiNetworkAdapterInfo info{};

                if (ConvertInterfaceLuidToGuid(&adapter->Luid, &info.Id) != NO_ERROR) continue;

                info.Name = adapter->FriendlyName != nullptr ? adapter->FriendlyName : L"";
                info.Description = adapter->Description != nullptr ? adapter->Description : L"";
                info.PhysicalAddress = FormatPhysicalAddress(adapter->PhysicalAddress, (std::min)(adapter->PhysicalAddressLength, static_cast<ULONG>(MAX_ADAPTER_ADDRESS_LENGTH)));

                info.IPv4InterfaceIndex = (adapter->Flags & IP_ADAPTER_IPV4_ENABLED) != 0 ? adapter->IfIndex : 0;
                info.IPv6InterfaceIndex = (adapter->Flags & IP_ADAPTER_IPV6_ENABLED) != 0 ? adapter->Ipv6IfIndex : 0;
                info.Metric = (std::min)(adapter->Ipv4Metric, adapter->Ipv6Metric);

                info.IsUp = adapter->OperStatus == IfOperStatusUp;
                info.SupportsMulticast = (adapter->Flags & IP_ADAPTER_NO_MULTICAST) == 0;

                for (auto unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
                {
                    if (unicast->Address.lpSockaddr == nullptr) continue;

                    // one still being checked for duplicates on the network cannot be used yet
                    if (unicast->DadState != IpDadStatePreferred && unicast->DadState != IpDadStateDeprecated) continue;

                    auto text = FormatAddress(unicast->Address.lpSockaddr);
                    if (text.empty()) continue;

                    if (unicast->Address.lpSockaddr->sa_family == AF_INET) info.IPv4Addresses.push_back(std::move(text));
                    else info.IPv6Addresses.push_back(std::move(text));
                }

                adapters.push_back(std::move(info));
            }
        }
        catch (...)
        {
            adapters.clear();
        }

        return adapters;
    }

    _Use_decl_annotations_
    bool TryGetMidiNetworkAdapter(GUID const& id, MidiNetworkAdapterInfo& found) noexcept
    {
        found = MidiNetworkAdapterInfo{};

        try
        {
            if (IsEqualGUID(id, GUID_NULL)) return false;

            for (auto& adapter : GetMidiNetworkAdapters())
            {
                if (IsEqualGUID(adapter.Id, id))
                {
                    found = std::move(adapter);
                    return true;
                }
            }
        }
        catch (...)
        {
            found = MidiNetworkAdapterInfo{};
        }

        return false;
    }

    _Use_decl_annotations_
    bool TryFindUsableMidiNetworkAdapter(
        std::vector<MidiNetworkAdapterInfo> const& adapters,
        GUID const& id,
        std::wstring const& physicalAddress,
        MidiNetworkAdapterInfo& found)
    {
        found = MidiNetworkAdapterInfo{};

        if (!IsEqualGUID(id, GUID_NULL))
        {
            auto const byId = std::find_if(adapters.begin(), adapters.end(),
                [&](MidiNetworkAdapterInfo const& adapter) { return IsEqualGUID(adapter.Id, id) != FALSE; });

            if (byId != adapters.end())
            {
                if (!byId->IsUsable()) return false;

                found = *byId;
                return true;
            }
        }

        if (physicalAddress.empty()) return false;

        auto const byAddress = std::find_if(adapters.begin(), adapters.end(),
            [&](MidiNetworkAdapterInfo const& adapter)
            {
                return adapter.IsUsable() && _wcsicmp(adapter.PhysicalAddress.c_str(), physicalAddress.c_str()) == 0;
            });

        if (byAddress == adapters.end()) return false;

        found = *byAddress;
        return true;
    }

    _Use_decl_annotations_
    std::wstring MidiNetworkAdapterIdToString(GUID const& id)
    {
        if (IsEqualGUID(id, GUID_NULL)) return {};

        wchar_t buffer[40]{};
        if (StringFromGUID2(id, buffer, ARRAYSIZE(buffer)) == 0) return {};

        return buffer;
    }

    _Use_decl_annotations_
    bool TryParseMidiNetworkAdapterId(std::wstring const& text, GUID& id) noexcept
    {
        id = GUID_NULL;

        try
        {
            std::wstring trimmed{ text };

            auto const notSpace = [](wchar_t const c) { return !std::iswspace(c); };
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), notSpace));
            trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), notSpace).base(), trimmed.end());

            if (trimmed.empty()) return true;

            if (trimmed.front() != L'{') trimmed = L"{" + trimmed + L"}";

            // IIDFromString, unlike CLSIDFromString, never goes to the registry to look up a name
            if (trimmed.size() != 38) return false;

            GUID parsed{};
            if (FAILED(IIDFromString(trimmed.c_str(), &parsed))) return false;

            id = parsed;
            return true;
        }
        catch (...)
        {
            id = GUID_NULL;
            return false;
        }
    }


    struct MidiNetworkChangeMonitorCallbacks
    {
        // Runs on a system thread, where nothing may block for long or throw
        static VOID NETIOAPI_API_ AddressChanged(
            _In_ PVOID context,
            _In_opt_ PMIB_UNICASTIPADDRESS_ROW /*row*/,
            _In_ MIB_NOTIFICATION_TYPE notificationType) noexcept
        {
            if (notificationType == MibInitialNotification) return;

            auto const monitor = static_cast<MidiNetworkChangeMonitor*>(context);
            if (monitor == nullptr || monitor->m_timer == nullptr) return;

            // Setting it again moves it later, which is what turns a burst into one callback
            LARGE_INTEGER due{};
            due.QuadPart = -static_cast<LONGLONG>(monitor->m_settleMilliseconds) * 10000;

            FILETIME dueTime{};
            dueTime.dwLowDateTime = due.LowPart;
            dueTime.dwHighDateTime = static_cast<DWORD>(due.HighPart);

            SetThreadpoolTimer(static_cast<PTP_TIMER>(monitor->m_timer), &dueTime, 0, 0);
        }

        static VOID CALLBACK Settled(
            _Inout_ PTP_CALLBACK_INSTANCE /*instance*/,
            _Inout_opt_ PVOID context,
            _Inout_ PTP_TIMER /*timer*/) noexcept
        {
            auto const monitor = static_cast<MidiNetworkChangeMonitor*>(context);
            if (monitor == nullptr || !monitor->m_onChanged) return;

            try
            {
                monitor->m_onChanged();
            }
            catch (...)
            {
                // a thread pool callback which throws ends the process
            }
        }
    };

    _Use_decl_annotations_
    bool MidiNetworkChangeMonitor::Start(ChangedHandler onChanged, uint32_t const settleMilliseconds) noexcept
    {
        if (m_notification != nullptr) return true;

        m_onChanged = std::move(onChanged);
        m_settleMilliseconds = settleMilliseconds;

        auto const timer = CreateThreadpoolTimer(&MidiNetworkChangeMonitorCallbacks::Settled, this, nullptr);
        if (timer == nullptr) return false;

        m_timer = timer;

        HANDLE notification{ nullptr };

        if (NotifyUnicastIpAddressChange(AF_UNSPEC, &MidiNetworkChangeMonitorCallbacks::AddressChanged, this, FALSE, &notification) != NO_ERROR)
        {
            CloseThreadpoolTimer(timer);
            m_timer = nullptr;

            return false;
        }

        m_notification = notification;

        return true;
    }

    void MidiNetworkChangeMonitor::Stop() noexcept
    {
        // Waits for a notification callback in progress, so none can set the timer after this
        if (m_notification != nullptr)
        {
            CancelMibChangeNotify2(static_cast<HANDLE>(m_notification));
            m_notification = nullptr;
        }

        if (m_timer != nullptr)
        {
            auto const timer = static_cast<PTP_TIMER>(m_timer);

            SetThreadpoolTimer(timer, nullptr, 0, 0);
            WaitForThreadpoolTimerCallbacks(timer, TRUE);
            CloseThreadpoolTimer(timer);

            m_timer = nullptr;
        }

        m_onChanged = nullptr;
    }
}
