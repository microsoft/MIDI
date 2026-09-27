// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Multicast DNS helpers for the rtpMIDI transport: the host name behind a remote
// address, and follow-up announcements for the hosts on this PC.
//
// Standalone, so the spike can test it without the service.
// ============================================================================

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "midi_dnssd_browser.h"
#include "rtpmidi_session.h"

namespace RtpMidiMdns
{
    // The host name of the advertisement that lists this address. Empty when none does, and when
    // advertisements from different hosts both list it, so a stale entry never names the wrong device.
    inline std::wstring FindHostNameForAddress(
        _In_ std::vector<WindowsMidiServicesInternal::MidiDnssdService> const& services,
        _In_ RtpMidi::PeerAddress const& address)
    {
        if (address.Family != 4 && address.Family != 6) return {};

        int const family = address.Family == 4 ? AF_INET : AF_INET6;
        size_t const length = address.Family == 4 ? 4 : 16;

        std::wstring found{};

        for (auto const& service : services)
        {
            if (service.HostName.empty()) continue;

            auto const& listed = address.Family == 4 ? service.IPv4Addresses : service.IPv6Addresses;

            // advertised addresses carry no scope, so a link-local match ignores the connection's
            bool const matches = std::any_of(listed.begin(), listed.end(), [&](std::wstring const& text)
            {
                std::array<uint8_t, 16> bytes{};
                return InetPtonW(family, text.c_str(), bytes.data()) == 1 && memcmp(bytes.data(), address.Bytes.data(), length) == 0;
            });

            if (!matches) continue;

            if (found.empty()) found = service.HostName;
            else if (_wcsicmp(found.c_str(), service.HostName.c_str()) != 0) return {};
        }

        return found;
    }


    // WORKAROUND for the Windows DNS client, until it is fixed.
    //
    // When a DNS-SD registration completes, the DNS client announces it once, where RFC 6762
    // section 8.3 asks for at least two announcements a second apart, and it sets the cache-flush
    // bit on the shared PTR record, which section 10.2 forbids. A device that misses that one
    // packet, which is easy on Wi-Fi, doesn't list this PC until it asks again, and a Mac can go a
    // long time without asking. The flush bit also makes every device that hears it drop the other
    // instances of the service type, including the hosts this PC registered earlier. Answers to
    // queries are correct, so only the announcements need help.
    //
    // So after each registration the transport repeats the PTR record of every host it has
    // registered, twice, without the flush bit. Only the PTR records: the SRV, TXT and address
    // records are unique, the DNS client owns them and answers for them correctly, and a copy
    // that differed from its own in any byte would flush the real one.

    // mDNS responses carrying one PTR record per label, such as
    // "_apple-midi._udp.local PTR Pete PC._apple-midi._udp.local", split so that none is larger
    // than maxPacketBytes. A label that is empty, longer than 63 bytes or holds a period is left out.
    inline std::vector<std::vector<uint8_t>> BuildPtrAnnouncements(
        _In_ std::string const& serviceType,
        _In_ std::vector<std::string> const& instanceLabels,
        _In_ uint32_t const ttlSeconds,
        _In_ size_t const maxPacketBytes)
    {
        // the owner name in wire form, one length byte before each label and a zero at the end
        std::vector<uint8_t> owner;

        for (size_t start = 0; start < serviceType.size(); )
        {
            auto const end = (std::min)(serviceType.find('.', start), serviceType.size());
            auto const labelLength = end - start;

            if (labelLength == 0 || labelLength > 63) return {};

            owner.push_back(static_cast<uint8_t>(labelLength));
            owner.insert(owner.end(), serviceType.begin() + start, serviceType.begin() + end);
            start = end + 1;
        }

        owner.push_back(0);
        if (owner.size() < 2 || owner.size() > 255) return {};

        constexpr uint8_t headerBytes = 12;

        // each record after a packet's first names its owner with a pointer back to the first
        constexpr uint16_t ownerPointer = 0xC000 | headerBytes;

        auto const put16 = [](std::vector<uint8_t>& out, uint16_t const value)
        {
            out.push_back(static_cast<uint8_t>(value >> 8));
            out.push_back(static_cast<uint8_t>(value));
        };

        std::vector<std::vector<uint8_t>> packets;
        std::vector<uint8_t> packet;
        uint16_t answers{ 0 };

        auto const finish = [&]()
        {
            if (answers == 0) return;

            packet[6] = static_cast<uint8_t>(answers >> 8);
            packet[7] = static_cast<uint8_t>(answers);

            packets.push_back(std::move(packet));
            packet.clear();
            answers = 0;
        };

        for (auto const& label : instanceLabels)
        {
            if (label.empty() || label.size() > 63 || label.find('.') != std::string::npos) continue;

            size_t const dataBytes = 1 + label.size() + 2;

            if (answers != 0 && packet.size() + 2 + 10 + dataBytes > maxPacketBytes) finish();

            if (answers == 0)
            {
                // ID 0, flags 0x8400: an authoritative response, with no questions (RFC 6762 section 18)
                packet = { 0, 0, 0x84, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
                packet.insert(packet.end(), owner.begin(), owner.end());
            }
            else
            {
                put16(packet, ownerPointer);
            }

            put16(packet, 12);                                              // PTR
            put16(packet, 1);                                               // IN, with the cache-flush bit clear
            put16(packet, static_cast<uint16_t>(ttlSeconds >> 16));
            put16(packet, static_cast<uint16_t>(ttlSeconds));
            put16(packet, static_cast<uint16_t>(dataBytes));

            packet.push_back(static_cast<uint8_t>(label.size()));
            packet.insert(packet.end(), label.begin(), label.end());
            put16(packet, ownerPointer);

            answers++;
        }

        finish();

        return packets;
    }


    struct AnnouncementResult
    {
        // interfaces that every packet went out on
        uint32_t IPv4Interfaces{ 0 };
        uint32_t IPv6Interfaces{ 0 };

        // the last Winsock or IP Helper error, or 0
        int LastError{ 0 };
    };

    namespace Details
    {
        inline uint32_t SendOnInterfaces(
            _In_ int const family,
            _In_ std::vector<ULONG> const& interfaces,
            _In_ std::vector<std::vector<uint8_t>> const& packets,
            _Inout_ int& lastError)
        {
            if (interfaces.empty() || packets.empty()) return 0;

            auto const socket = WSASocketW(family, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, 0);
            if (socket == INVALID_SOCKET)
            {
                lastError = WSAGetLastError();
                return 0;
            }

            BOOL reuse = TRUE;
            setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char const*>(&reuse), sizeof(reuse));

            // RFC 6762 section 11: a hop limit of 255 tells receivers the packet came from the local link
            DWORD hops = 255;
            sockaddr_storage local{};
            int localLength{ 0 };

            if (family == AF_INET)
            {
                setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<char const*>(&hops), sizeof(hops));

                auto& v4 = reinterpret_cast<sockaddr_in&>(local);
                v4.sin_family = AF_INET;
                v4.sin_addr.s_addr = htonl(INADDR_ANY);
                v4.sin_port = htons(5353);
                localLength = sizeof(sockaddr_in);
            }
            else
            {
                DWORD v6Only = 1;
                setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char const*>(&v6Only), sizeof(v6Only));
                setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, reinterpret_cast<char const*>(&hops), sizeof(hops));

                auto& v6 = reinterpret_cast<sockaddr_in6&>(local);
                v6.sin6_family = AF_INET6;
                v6.sin6_addr = in6addr_any;
                v6.sin6_port = htons(5353);
                localLength = sizeof(sockaddr_in6);
            }

            // Receivers ignore mDNS responses from any port but 5353 (RFC 6762 section 6), and the
            // port is shared with the DNS client. The socket is closed as soon as the packets are
            // out, because while it is open the network stack may hand it a unicast packet meant
            // for the DNS client.
            if (bind(socket, reinterpret_cast<sockaddr const*>(&local), localLength) == SOCKET_ERROR)
            {
                lastError = WSAGetLastError();
                closesocket(socket);
                return 0;
            }

            uint32_t sentOn{ 0 };

            for (auto const index : interfaces)
            {
                sockaddr_storage destination{};
                int destinationLength{ 0 };
                int optionResult{ SOCKET_ERROR };

                if (family == AF_INET)
                {
                    // an index in network byte order, which Winsock tells from an address by its 0 first byte
                    DWORD networkOrderIndex = htonl(index);
                    optionResult = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<char const*>(&networkOrderIndex), sizeof(networkOrderIndex));

                    auto& v4 = reinterpret_cast<sockaddr_in&>(destination);
                    v4.sin_family = AF_INET;
                    v4.sin_port = htons(5353);
                    InetPtonW(AF_INET, L"224.0.0.251", &v4.sin_addr);
                    destinationLength = sizeof(sockaddr_in);
                }
                else
                {
                    DWORD hostOrderIndex = index;
                    optionResult = setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_IF, reinterpret_cast<char const*>(&hostOrderIndex), sizeof(hostOrderIndex));

                    auto& v6 = reinterpret_cast<sockaddr_in6&>(destination);
                    v6.sin6_family = AF_INET6;
                    v6.sin6_port = htons(5353);
                    v6.sin6_scope_id = index;
                    InetPtonW(AF_INET6, L"ff02::fb", &v6.sin6_addr);
                    destinationLength = sizeof(sockaddr_in6);
                }

                if (optionResult == SOCKET_ERROR)
                {
                    lastError = WSAGetLastError();
                    continue;
                }

                bool everyPacket{ true };

                for (auto const& packet : packets)
                {
                    auto const sent = sendto(socket, reinterpret_cast<char const*>(packet.data()), static_cast<int>(packet.size()), 0,
                        reinterpret_cast<sockaddr const*>(&destination), destinationLength);

                    if (sent != static_cast<int>(packet.size()))
                    {
                        lastError = WSAGetLastError();
                        everyPacket = false;
                    }
                }

                if (everyPacket) sentOn++;
            }

            closesocket(socket);

            return sentOn;
        }
    }

    // Multicasts the packets on every interface that is up and carries multicast, over IPv4 and
    // IPv6. Winsock must already be started.
    inline AnnouncementResult SendAnnouncements(_In_ std::vector<std::vector<uint8_t>> const& packets)
    {
        AnnouncementResult result{};
        if (packets.empty()) return result;

        ULONG const flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;

        std::vector<uint8_t> buffer;
        ULONG size = 16 * 1024;
        ULONG status = ERROR_BUFFER_OVERFLOW;

        // the adapter list can grow between the call that sizes the buffer and the one that fills it
        for (int attempt = 0; attempt < 3 && status == ERROR_BUFFER_OVERFLOW; attempt++)
        {
            buffer.resize(size);
            status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
        }

        if (status != NO_ERROR)
        {
            result.LastError = static_cast<int>(status);
            return result;
        }

        std::vector<ULONG> ipv4Interfaces;
        std::vector<ULONG> ipv6Interfaces;

        for (auto adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES const*>(buffer.data()); adapter != nullptr; adapter = adapter->Next)
        {
            if (adapter->OperStatus != IfOperStatusUp) continue;
            if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->IfType == IF_TYPE_TUNNEL || adapter->IfType == IF_TYPE_PPP) continue;
            if ((adapter->Flags & IP_ADAPTER_NO_MULTICAST) != 0) continue;

            bool haveIPv4{ false };
            bool haveIPv6{ false };

            for (auto unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
            {
                if (unicast->Address.lpSockaddr == nullptr) continue;

                if (unicast->Address.lpSockaddr->sa_family == AF_INET) haveIPv4 = true;
                else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) haveIPv6 = true;
            }

            if (haveIPv4 && adapter->IfIndex != 0 && (adapter->Flags & IP_ADAPTER_IPV4_ENABLED) != 0) ipv4Interfaces.push_back(adapter->IfIndex);
            if (haveIPv6 && adapter->Ipv6IfIndex != 0 && (adapter->Flags & IP_ADAPTER_IPV6_ENABLED) != 0) ipv6Interfaces.push_back(adapter->Ipv6IfIndex);
        }

        result.IPv4Interfaces = Details::SendOnInterfaces(AF_INET, ipv4Interfaces, packets, result.LastError);
        result.IPv6Interfaces = Details::SendOnInterfaces(AF_INET6, ipv6Interfaces, packets, result.LastError);

        return result;
    }
}
