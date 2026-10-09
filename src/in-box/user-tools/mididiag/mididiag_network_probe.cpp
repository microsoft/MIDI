// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// See mididiag_network_probe.h. The precompiled header leaves winsock.h out of windows.h, so
// the Winsock 2 headers can follow it here.

#include "pch.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <windns.h>

#include <atomic>

#include "mididiag_network_probe.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "dnsapi.lib")

namespace mididiag::netprobe
{
    namespace
    {
        // how long a lookup that was given up on gets to report that it stopped
        constexpr DWORD LookupCancelWaitMilliseconds{ 2000 };

        std::wstring AdapterKindName(_In_ IFTYPE const type)
        {
            switch (type)
            {
            case IF_TYPE_ETHERNET_CSMACD:   return L"ethernet";
            case IF_TYPE_IEEE80211:         return L"wifi";
            case IF_TYPE_PPP:               return L"ppp";
            default:                        return L"other";
            }
        }

        bool TryDescribeAddress(_In_ SOCKET_ADDRESS const& address, _Out_ AdapterAddress& described)
        {
            described = {};

            if (address.lpSockaddr == nullptr)
            {
                return false;
            }

            wchar_t text[INET6_ADDRSTRLEN]{};

            if (address.lpSockaddr->sa_family == AF_INET && address.iSockaddrLength >= static_cast<INT>(sizeof(sockaddr_in)))
            {
                auto const ipv4 = reinterpret_cast<sockaddr_in const*>(address.lpSockaddr);

                if (::InetNtopW(AF_INET, &ipv4->sin_addr, text, ARRAYSIZE(text)) == nullptr)
                {
                    return false;
                }

                described.Kind = AddressKind::IPv4;
                described.Autoconfigured = ipv4->sin_addr.S_un.S_un_b.s_b1 == 169 && ipv4->sin_addr.S_un.S_un_b.s_b2 == 254;
            }
            else if (address.lpSockaddr->sa_family == AF_INET6 && address.iSockaddrLength >= static_cast<INT>(sizeof(sockaddr_in6)))
            {
                auto const ipv6 = reinterpret_cast<sockaddr_in6 const*>(address.lpSockaddr);

                if (::InetNtopW(AF_INET6, &ipv6->sin6_addr, text, ARRAYSIZE(text)) == nullptr)
                {
                    return false;
                }

                if (IN6_IS_ADDR_LINKLOCAL(&ipv6->sin6_addr))
                {
                    described.Kind = AddressKind::IPv6LinkLocal;
                }
                else if ((ipv6->sin6_addr.u.Byte[0] & 0xFE) == 0xFC)
                {
                    // fc00::/7, the IPv6 equivalent of a private IPv4 address
                    described.Kind = AddressKind::IPv6UniqueLocal;
                }
                else
                {
                    described.Kind = AddressKind::IPv6Global;
                }
            }
            else
            {
                return false;
            }

            described.Text = text;

            return true;
        }

        uint64_t MillisecondsSince(_In_ std::chrono::steady_clock::time_point const start)
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count());
        }

        // Shared by the caller and the completion callback, so a callback that comes after the
        // caller gave up still has somewhere to write
        struct LookupState
        {
            std::wstring QueryName{};
            DNS_SERVICE_RESOLVE_REQUEST Request{};
            DNS_SERVICE_CANCEL Cancel{};

            // Set by the callback once it has written the fields below. The caller reads them
            // only after it sees this set.
            wil::unique_event Done{};

            DWORD Status{ ERROR_TIMEOUT };
            bool HasInstance{ false };
            std::wstring HostName{};
            uint16_t Port{ 0 };
        };

        // DNS-SD calls this once, with the answer or with the reason there is none. context is
        // the callback's own reference to the state, made for it before the lookup started.
        VOID WINAPI OnLookupComplete(_In_ DWORD const status, _In_ PVOID const context, _In_opt_ PDNS_SERVICE_INSTANCE const instance)
        {
            std::unique_ptr<std::shared_ptr<LookupState>> const reference{ static_cast<std::shared_ptr<LookupState>*>(context) };
            auto const& state = *reference;

            state->Status = status;
            state->HasInstance = instance != nullptr;

            if (instance != nullptr)
            {
                state->Port = instance->wPort;

                try
                {
                    state->HostName = instance->pszHostName == nullptr ? L"" : instance->pszHostName;
                }
                catch (...)
                {
                    // the answer is still reported, without the host name
                }

                ::DnsServiceFreeInstance(instance);
            }

            state->Done.SetEvent();
        }

        ServiceLookupResult LookUp(_In_ std::wstring const& fullName, _In_ uint32_t const timeoutMilliseconds)
        {
            ServiceLookupResult result{};
            result.Status = ERROR_TIMEOUT;

            auto const state = std::make_shared<LookupState>();
            state->Done.create(wil::EventOptions::ManualReset);

            // kept with the state, in case the DNS client reads the request after the call returns
            state->QueryName = fullName;
            state->Request.Version = DNS_QUERY_REQUEST_VERSION1;
            state->Request.InterfaceIndex = 0;
            state->Request.QueryName = state->QueryName.data();
            state->Request.pResolveCompletionCallback = &OnLookupComplete;

            // owed before the call, because the callback can come before the call returns
            auto callbackReference = std::make_unique<std::shared_ptr<LookupState>>(state);
            state->Request.pQueryContext = callbackReference.get();

            auto const start = std::chrono::steady_clock::now();
            auto const status = ::DnsServiceResolve(&state->Request, &state->Cancel);

            if (status != DNS_REQUEST_PENDING)
            {
                // nothing started, so no callback is coming and the reference is freed here
                result.Status = status;
                result.ElapsedMilliseconds = MillisecondsSince(start);

                return result;
            }

            // the callback frees it now
            static_cast<void>(callbackReference.release());

            if (!state->Done.wait(timeoutMilliseconds))
            {
                // Whether the callback still comes after this is not documented, so its
                // reference keeps the state alive until it does.
                ::DnsServiceResolveCancel(&state->Cancel);
                state->Done.wait(LookupCancelWaitMilliseconds);
            }

            result.ElapsedMilliseconds = MillisecondsSince(start);

            if (!state->Done.is_signaled())
            {
                return result;
            }

            if (state->Status == ERROR_SUCCESS && state->HasInstance)
            {
                result.Answered = true;
                result.Status = ERROR_SUCCESS;
                result.HostName = state->HostName;
                result.Port = state->Port;
            }
            else if (state->Status != ERROR_SUCCESS && state->Status != ERROR_CANCELLED)
            {
                result.Status = state->Status;
            }

            return result;
        }
    }

    _Use_decl_annotations_
    bool TryGetAdapters(std::vector<AdapterInfo>& adapters) noexcept
    {
        adapters.clear();

        try
        {
            ULONG const flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

            // backed by 64-bit storage so the structures inside it are correctly aligned
            std::vector<uint64_t> buffer{};
            ULONG size{ 16 * 1024 };
            ULONG status{ ERROR_BUFFER_OVERFLOW };

            // the list can grow between one call and the next
            for (uint32_t attempt = 0; attempt < 3 && status == ERROR_BUFFER_OVERFLOW; attempt++)
            {
                buffer.resize(size / sizeof(uint64_t) + 1);
                size = static_cast<ULONG>(buffer.size() * sizeof(uint64_t));

                status = ::GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
            }

            if (status == ERROR_NO_DATA)
            {
                return true;
            }

            if (status != ERROR_SUCCESS)
            {
                return false;
            }

            for (auto adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES const*>(buffer.data()); adapter != nullptr; adapter = adapter->Next)
            {
                if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->IfType == IF_TYPE_TUNNEL)
                {
                    continue;
                }

                AdapterInfo info{};

                // the interface GUID, in braces
                for (auto character = adapter->AdapterName; character != nullptr && *character != '\0'; character++)
                {
                    info.Id.push_back(static_cast<wchar_t>(*character));
                }

                info.InterfaceIndex = adapter->IfIndex != 0 ? adapter->IfIndex : adapter->Ipv6IfIndex;
                info.Name = adapter->FriendlyName == nullptr ? L"" : adapter->FriendlyName;
                info.Description = adapter->Description == nullptr ? L"" : adapter->Description;
                info.Kind = AdapterKindName(adapter->IfType);
                info.IsUp = adapter->OperStatus == IfOperStatusUp;
                info.SupportsMulticast = (adapter->Flags & IP_ADAPTER_NO_MULTICAST) == 0;
                info.DhcpEnabled = (adapter->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;

                bool const ipv4 = (adapter->Flags & IP_ADAPTER_IPV4_ENABLED) != 0;
                bool const ipv6 = (adapter->Flags & IP_ADAPTER_IPV6_ENABLED) != 0;

                info.Metric = ipv4 && ipv6 ? (std::min)(adapter->Ipv4Metric, adapter->Ipv6Metric) :
                    ipv4 ? adapter->Ipv4Metric : adapter->Ipv6Metric;

                MIB_IF_ROW2 row{};
                row.InterfaceLuid = adapter->Luid;

                if (::GetIfEntry2(&row) == NO_ERROR)
                {
                    info.IsHardware = row.InterfaceAndOperStatusFlags.HardwareInterface != FALSE;
                }

                for (auto unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
                {
                    if (unicast->DadState != IpDadStatePreferred && unicast->DadState != IpDadStateDeprecated)
                    {
                        continue;
                    }

                    AdapterAddress address{};

                    if (!TryDescribeAddress(unicast->Address, address))
                    {
                        continue;
                    }

                    address.Deprecated = unicast->DadState == IpDadStateDeprecated;

                    // Windows marks its privacy addresses with a random suffix. A link-local
                    // address can have a random suffix too, and it is not one of those.
                    address.Temporary = (address.Kind == AddressKind::IPv6Global || address.Kind == AddressKind::IPv6UniqueLocal) &&
                        unicast->SuffixOrigin == IpSuffixOriginRandom;

                    info.Addresses.push_back(std::move(address));
                }

                adapters.push_back(std::move(info));
            }

            return true;
        }
        catch (...)
        {
            adapters.clear();

            return false;
        }
    }

    _Use_decl_annotations_
    bool TryGetUdpPortUsers(uint16_t const port, std::vector<UdpPortUser>& users) noexcept
    {
        users.clear();

        try
        {
            std::map<uint32_t, UdpPortUser> byProcess{};
            bool anyRead{ false };

            for (ULONG const family : { static_cast<ULONG>(AF_INET), static_cast<ULONG>(AF_INET6) })
            {
                std::vector<uint64_t> buffer{};
                DWORD size{ 0 };
                DWORD status = ::GetExtendedUdpTable(nullptr, &size, FALSE, family, UDP_TABLE_OWNER_PID, 0);

                // sockets can open between one call and the next
                for (uint32_t attempt = 0; attempt < 3 && status == ERROR_INSUFFICIENT_BUFFER; attempt++)
                {
                    buffer.resize(size / sizeof(uint64_t) + 1);
                    size = static_cast<DWORD>(buffer.size() * sizeof(uint64_t));

                    status = ::GetExtendedUdpTable(buffer.data(), &size, FALSE, family, UDP_TABLE_OWNER_PID, 0);
                }

                if (status != NO_ERROR || buffer.empty())
                {
                    continue;
                }

                anyRead = true;

                if (family == AF_INET)
                {
                    auto const table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID const*>(buffer.data());

                    for (DWORD i = 0; i < table->dwNumEntries; i++)
                    {
                        auto const& entry = table->table[i];

                        if (::ntohs(static_cast<u_short>(entry.dwLocalPort)) == port)
                        {
                            byProcess[entry.dwOwningPid].IPv4Sockets++;
                        }
                    }
                }
                else
                {
                    auto const table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID const*>(buffer.data());

                    for (DWORD i = 0; i < table->dwNumEntries; i++)
                    {
                        auto const& entry = table->table[i];

                        if (::ntohs(static_cast<u_short>(entry.dwLocalPort)) == port)
                        {
                            byProcess[entry.dwOwningPid].IPv6Sockets++;
                        }
                    }
                }
            }

            for (auto& [processId, user] : byProcess)
            {
                user.ProcessId = processId;
                users.push_back(user);
            }

            return anyRead;
        }
        catch (...)
        {
            users.clear();

            return false;
        }
    }

    _Use_decl_annotations_
    ServiceLookupResult LookUpServiceInstance(std::wstring const& fullName, uint32_t const timeoutMilliseconds) noexcept
    {
        try
        {
            return LookUp(fullName, timeoutMilliseconds);
        }
        catch (...)
        {
            ServiceLookupResult result{};
            result.Status = ERROR_GEN_FAILURE;

            return result;
        }
    }
}
