// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. A control/data UDP port pair for one AppleMIDI session.
//
// Dual-stack IPv6 sockets, so one socket per port serves IPv4 peers too (they arrive as
// IPv4-mapped addresses and are normalized back to IPv4 here). Each socket has its own receive
// thread, which is what the RTP-MIDI data path wants in a real transport too.
// ============================================================================

#pragma once

#include "spike_common.h"

#include <mswsock.h>

#include <functional>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace Spike
{
    inline RtpMidi::PeerAddress FromSockaddr(sockaddr_storage const& storage)
    {
        RtpMidi::PeerAddress address{};

        if (storage.ss_family == AF_INET)
        {
            auto const& v4 = reinterpret_cast<sockaddr_in const&>(storage);
            address.Family = 4;
            memcpy(address.Bytes.data(), &v4.sin_addr, 4);
            address.Port = ntohs(v4.sin_port);
        }
        else if (storage.ss_family == AF_INET6)
        {
            auto const& v6 = reinterpret_cast<sockaddr_in6 const&>(storage);
            address.Port = ntohs(v6.sin6_port);

            if (IN6_IS_ADDR_V4MAPPED(&v6.sin6_addr))
            {
                address.Family = 4;
                memcpy(address.Bytes.data(), &v6.sin6_addr.u.Byte[12], 4);
            }
            else
            {
                address.Family = 6;
                memcpy(address.Bytes.data(), &v6.sin6_addr, 16);
                address.ScopeId = v6.sin6_scope_id;
            }
        }

        return address;
    }

    // Always an IPv6 socket address, because the sockets are dual-stack.
    inline sockaddr_in6 ToSockaddr(RtpMidi::PeerAddress const& address)
    {
        sockaddr_in6 v6{};
        v6.sin6_family = AF_INET6;
        v6.sin6_port = htons(address.Port);

        if (address.Family == 4)
        {
            v6.sin6_addr.u.Byte[10] = 0xFF;
            v6.sin6_addr.u.Byte[11] = 0xFF;
            memcpy(&v6.sin6_addr.u.Byte[12], address.Bytes.data(), 4);
        }
        else
        {
            memcpy(&v6.sin6_addr, address.Bytes.data(), 16);
            v6.sin6_scope_id = address.ScopeId;
        }

        return v6;
    }

    inline bool TryParseAddress(std::wstring const& text, uint16_t port, RtpMidi::PeerAddress& address)
    {
        address = RtpMidi::PeerAddress{};

        in_addr v4{};
        if (InetPtonW(AF_INET, text.c_str(), &v4) == 1)
        {
            address.Family = 4;
            memcpy(address.Bytes.data(), &v4, 4);
            address.Port = port;
            return true;
        }

        // an IPv6 literal may carry a %scope suffix
        std::wstring literal = text;
        uint32_t scope = 0;
        auto const percent = literal.find(L'%');
        if (percent != std::wstring::npos)
        {
            scope = static_cast<uint32_t>(wcstoul(literal.c_str() + percent + 1, nullptr, 10));
            literal.resize(percent);
        }

        in6_addr v6{};
        if (InetPtonW(AF_INET6, literal.c_str(), &v6) == 1)
        {
            address.Family = 6;
            memcpy(address.Bytes.data(), &v6, 16);
            address.ScopeId = scope;
            address.Port = port;
            return true;
        }

        return false;
    }

    class UdpSocket
    {
    public:
        using ReceiveHandler = std::function<void(RtpMidi::PeerAddress const& from, uint8_t const* data, size_t size)>;

        UdpSocket() = default;
        ~UdpSocket() { Close(); }

        UdpSocket(UdpSocket const&) = delete;
        UdpSocket& operator=(UdpSocket const&) = delete;

        // loopbackOnly binds ::1, which never listens on the network and never needs a firewall rule
        bool Bind(uint16_t port, int& error, bool loopbackOnly = false)
        {
            auto const socket = WSASocketW(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
            if (socket == INVALID_SOCKET) { error = WSAGetLastError(); return false; }

            m_socket = socket;

            DWORD off = 0;
            setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char const*>(&off), sizeof(off));

            // nobody else may bind the same port while we hold it
            DWORD on = 1;
            setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char const*>(&on), sizeof(on));

            // Windows reports an ICMP port unreachable from an earlier send as a WSAECONNRESET on
            // the next receive. One peer going away must not break the socket for everyone else.
            BOOL reportReset = FALSE;
            DWORD returned = 0;
            WSAIoctl(socket, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &returned, nullptr, nullptr);

            sockaddr_in6 local{};
            local.sin6_family = AF_INET6;
            local.sin6_addr = loopbackOnly ? in6addr_loopback : in6addr_any;
            local.sin6_port = htons(port);

            if (bind(socket, reinterpret_cast<sockaddr const*>(&local), sizeof(local)) == SOCKET_ERROR)
            {
                error = WSAGetLastError();
                Close();
                return false;
            }

            sockaddr_in6 bound{};
            int length = sizeof(bound);
            getsockname(socket, reinterpret_cast<sockaddr*>(&bound), &length);
            m_port = ntohs(bound.sin6_port);

            return true;
        }

        uint16_t Port() const { return m_port; }

        void StartReceiving(ReceiveHandler handler)
        {
            m_handler = std::move(handler);
            m_thread = std::thread([this]() { ReceiveLoop(); });
        }

        bool Send(RtpMidi::PeerAddress const& to, std::vector<uint8_t> const& datagram)
        {
            auto const socket = m_socket.load();
            if (socket == INVALID_SOCKET) return false;

            auto const address = ToSockaddr(to);
            auto const sent = sendto(socket, reinterpret_cast<char const*>(datagram.data()), static_cast<int>(datagram.size()), 0,
                reinterpret_cast<sockaddr const*>(&address), sizeof(address));

            return sent == static_cast<int>(datagram.size());
        }

        void Close()
        {
            auto const socket = m_socket.exchange(INVALID_SOCKET);

            if (socket != INVALID_SOCKET) closesocket(socket);

            if (m_thread.joinable() && m_thread.get_id() != std::this_thread::get_id()) m_thread.join();
        }

    private:
        void ReceiveLoop()
        {
            std::vector<uint8_t> buffer(65536);

            for (;;)
            {
                auto const socket = m_socket.load();
                if (socket == INVALID_SOCKET) break;

                sockaddr_storage from{};
                int fromLength = sizeof(from);

                auto const received = recvfrom(socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                    reinterpret_cast<sockaddr*>(&from), &fromLength);

                if (received == SOCKET_ERROR)
                {
                    auto const error = WSAGetLastError();
                    if (error == WSAECONNRESET || error == WSAEMSGSIZE) continue;
                    break;
                }

                if (received > 0 && m_handler) m_handler(FromSockaddr(from), buffer.data(), static_cast<size_t>(received));
            }
        }

        std::atomic<SOCKET> m_socket{ INVALID_SOCKET };
        uint16_t m_port{ 0 };
        ReceiveHandler m_handler;
        std::thread m_thread;
    };

    // AppleMIDI needs the data port to be the control port plus one.
    class PortPair
    {
    public:
        bool Bind(uint16_t preferredControlPort, uint16_t searchFrom, uint16_t searchTo, std::string& failure, bool loopbackOnly = false)
        {
            int error = 0;
            m_loopbackOnly = loopbackOnly;

            if (preferredControlPort != 0)
            {
                if (TryBind(preferredControlPort, error)) return true;
                failure = "port " + std::to_string(preferredControlPort) + " or " + std::to_string(preferredControlPort + 1) + " is in use (error " + std::to_string(error) + ")";
                return false;
            }

            for (uint32_t port = searchFrom; port + 1 <= searchTo; port += 2)
            {
                if (TryBind(static_cast<uint16_t>(port), error)) return true;
            }

            failure = "no free adjacent port pair between " + std::to_string(searchFrom) + " and " + std::to_string(searchTo);
            return false;
        }

        UdpSocket& Control() { return m_control; }
        UdpSocket& Data() { return m_data; }

        void Close()
        {
            m_control.Close();
            m_data.Close();
        }

    private:
        bool TryBind(uint16_t port, int& error)
        {
            if (!m_control.Bind(port, error, m_loopbackOnly)) return false;

            if (!m_data.Bind(static_cast<uint16_t>(port + 1), error, m_loopbackOnly))
            {
                m_control.Close();
                return false;
            }

            return true;
        }

        UdpSocket m_control;
        UdpSocket m_data;
        bool m_loopbackOnly{ false };
    };
}
