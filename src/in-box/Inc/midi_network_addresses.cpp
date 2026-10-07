// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// See midi_network_addresses.h. winsock2.h has to come before windows.h, which is why this lives
// in its own translation unit rather than in the header.
// ============================================================================

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <windows.h>

#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

#include "midi_network_addresses.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace WindowsMidiServicesInternal
{
    namespace
    {
        // Far more than any device has. A device can advertise any number, and each costs work.
        constexpr size_t MaxSortedAddressCount{ 64 };

        class EventHandle
        {
        public:
            EventHandle() : m_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}
            ~EventHandle() { if (m_handle != nullptr) CloseHandle(m_handle); }

            EventHandle(_In_ EventHandle const&) = delete;
            EventHandle& operator=(_In_ EventHandle const&) = delete;

            HANDLE Get() const noexcept { return m_handle; }

        private:
            HANDLE m_handle{ nullptr };
        };

        // A name lookup needs Winsock up. The service and the transports start it elsewhere too,
        // but this must not depend on that.
        struct WinsockScope
        {
            bool Started{ false };

            WinsockScope()
            {
                WSADATA data{ };

                Started = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
            }

            ~WinsockScope()
            {
                if (Started) WSACleanup();
            }

            WinsockScope(_In_ WinsockScope const&) = delete;
            WinsockScope& operator=(_In_ WinsockScope const&) = delete;
        };

        // IPv4 comes back as an IPv4-mapped IPv6 address, the only form CreateSortedAddressPairs
        // takes. IPv6 may carry a %scope, the index of the adapter a link-local address is on.
        bool TryParseAddress(_In_ std::wstring const& text, _Out_ SOCKADDR_IN6& address)
        {
            address = SOCKADDR_IN6{};
            address.sin6_family = AF_INET6;

            IN_ADDR v4{};

            if (InetPtonW(AF_INET, text.c_str(), &v4) == 1)
            {
                address.sin6_addr.u.Byte[10] = 0xFF;
                address.sin6_addr.u.Byte[11] = 0xFF;
                memcpy(&address.sin6_addr.u.Byte[12], &v4, sizeof(v4));

                return true;
            }

            std::wstring literal{ text };
            auto const percent = literal.find(L'%');

            if (percent != std::wstring::npos)
            {
                auto const scopeText = literal.c_str() + percent + 1;
                wchar_t* end{ nullptr };
                auto const scope = wcstoul(scopeText, &end, 10);

                if (end == scopeText || *end != L'\0') return false;

                address.sin6_scope_id = scope;
                literal.resize(percent);
            }

            return InetPtonW(AF_INET6, literal.c_str(), &address.sin6_addr) == 1;
        }

        // A scope of zero on either side matches any, because Windows may fill in one it was not given
        bool SameAddress(_In_ SOCKADDR_IN6 const& left, _In_ SOCKADDR_IN6 const& right) noexcept
        {
            if (memcmp(&left.sin6_addr, &right.sin6_addr, sizeof(left.sin6_addr)) != 0) return false;

            return left.sin6_scope_id == 0 || right.sin6_scope_id == 0 || left.sin6_scope_id == right.sin6_scope_id;
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

                if (IN6_IS_ADDR_V4MAPPED(&v6->sin6_addr))
                {
                    if (InetNtopW(AF_INET, &v6->sin6_addr.u.Byte[12], buffer, ARRAYSIZE(buffer)) == nullptr) return {};

                    return buffer;
                }

                if (InetNtopW(AF_INET6, &v6->sin6_addr, buffer, ARRAYSIZE(buffer)) == nullptr) return {};

                std::wstring text{ buffer };

                if (IN6_IS_ADDR_LINKLOCAL(&v6->sin6_addr) && v6->sin6_scope_id != 0)
                {
                    text += L"%" + std::to_wstring(v6->sin6_scope_id);
                }

                return text;
            }

            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<std::wstring> SortMidiNetworkAddresses(std::vector<std::wstring> const& addresses)
    {
        std::vector<std::wstring> usable{};
        std::vector<SOCKADDR_IN6> parsed{};

        for (auto const& text : addresses)
        {
            if (usable.size() >= MaxSortedAddressCount) break;

            SOCKADDR_IN6 address{};
            if (!TryParseAddress(text, address)) continue;

            if (IN6_IS_ADDR_LINKLOCAL(&address.sin6_addr) && address.sin6_scope_id == 0) continue;

            bool duplicate{ false };

            for (auto const& earlier : parsed)
            {
                if (memcmp(&earlier.sin6_addr, &address.sin6_addr, sizeof(address.sin6_addr)) == 0 && earlier.sin6_scope_id == address.sin6_scope_id)
                {
                    duplicate = true;
                    break;
                }
            }

            if (duplicate) continue;

            usable.push_back(text);
            parsed.push_back(address);
        }

        // nothing to choose between, and one Windows can't place is still the only one to try
        if (usable.size() < 2) return usable;

        PSOCKADDR_IN6_PAIR pairs{ nullptr };
        ULONG pairCount{ 0 };

        auto const status = CreateSortedAddressPairs(nullptr, 0, parsed.data(), static_cast<ULONG>(parsed.size()), 0, &pairs, &pairCount);

        if (status != NO_ERROR || pairs == nullptr) return usable;

        std::unique_ptr<SOCKADDR_IN6_PAIR, decltype(&FreeMibTable)> const ownedPairs{ pairs, &FreeMibTable };

        std::vector<std::wstring> sorted{};
        std::vector<bool> taken(usable.size(), false);

        for (ULONG i = 0; i < pairCount; i++)
        {
            auto const destination = pairs[i].DestinationAddress;
            if (destination == nullptr) continue;

            for (size_t j = 0; j < parsed.size(); j++)
            {
                if (!taken[j] && SameAddress(parsed[j], *destination))
                {
                    taken[j] = true;
                    sorted.push_back(usable[j]);
                    break;
                }
            }
        }

        return sorted.empty() ? usable : sorted;
    }

    _Use_decl_annotations_
    std::vector<std::wstring> ResolveMidiNetworkHostName(
        std::wstring const& hostName,
        uint32_t const timeoutSeconds,
        std::stop_token const& stopToken)
    {
        std::vector<std::wstring> addresses{};

        if (hostName.empty() || stopToken.stop_requested()) return addresses;

        // first, so it is the last thing to go
        WinsockScope winsock;

        if (!winsock.Started) return addresses;

        EventHandle resolved;
        EventHandle stopped;

        if (resolved.Get() == nullptr || stopped.Get() == nullptr) return addresses;

        // A service stop cancels the lookup, so a slow or missing DNS server can't hold it up
        HANDLE const stoppedEvent = stopped.Get();
        std::stop_callback const onStop{ stopToken, [stoppedEvent]() noexcept { SetEvent(stoppedEvent); } };

        ADDRINFOEXW hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        PADDRINFOEXW results{ nullptr };
        OVERLAPPED overlapped{};
        overlapped.hEvent = resolved.Get();
        HANDLE cancel{ nullptr };
        timeval timeout{ static_cast<long>(timeoutSeconds), 0 };

        auto status = GetAddrInfoExW(hostName.c_str(), nullptr, NS_ALL, nullptr, &hints, &results, &timeout, &overlapped, nullptr, &cancel);

        if (status == WSA_IO_PENDING)
        {
            HANDLE const handles[]{ resolved.Get(), stopped.Get() };

            if (WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE) != WAIT_OBJECT_0)
            {
                // canceled or not, the lookup has to finish before the buffers it writes go away
                GetAddrInfoExCancel(&cancel);
                WaitForSingleObject(resolved.Get(), INFINITE);
            }

            status = GetAddrInfoExOverlappedResult(&overlapped);
        }

        // only once the lookup is over, because until then it may still write here
        std::unique_ptr<ADDRINFOEXW, decltype(&FreeAddrInfoExW)> const ownedResults{ results, &FreeAddrInfoExW };

        if (status == NO_ERROR)
        {
            for (auto result = results; result != nullptr; result = result->ai_next)
            {
                if (result->ai_addr == nullptr) continue;

                auto const text = FormatAddress(result->ai_addr);
                if (text.empty()) continue;

                bool listed{ false };

                for (auto const& earlier : addresses)
                {
                    if (earlier == text)
                    {
                        listed = true;
                        break;
                    }
                }

                if (!listed) addresses.push_back(text);
            }
        }

        return addresses;
    }

    _Use_decl_annotations_
    std::wstring ChooseMidiNetworkAddress(
        std::vector<std::wstring> const& addresses,
        std::wstring const& connectedAddress,
        uint32_t const unansweredAttempts)
    {
        if (addresses.empty()) return {};

        std::vector<size_t> order{};
        size_t connected{ addresses.size() };

        if (!connectedAddress.empty())
        {
            for (size_t i = 0; i < addresses.size(); i++)
            {
                if (_wcsicmp(addresses[i].c_str(), connectedAddress.c_str()) == 0)
                {
                    connected = i;
                    order.push_back(i);
                    break;
                }
            }
        }

        for (size_t i = 0; i < addresses.size(); i++)
        {
            if (i != connected) order.push_back(i);
        }

        return addresses[order[unansweredAttempts % order.size()]];
    }

    _Use_decl_annotations_
    bool IsNextMidiNetworkAddressUntried(uint32_t const unansweredAttempts, uint32_t const addressCount) noexcept
    {
        return addressCount > 1 && unansweredAttempts % addressCount != 0;
    }
}
