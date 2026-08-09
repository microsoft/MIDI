// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

namespace NetworkMidiTest
{
    WinsockScope::WinsockScope()
    {
        WSADATA data{ };

        m_initialized = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
    }

    WinsockScope::~WinsockScope()
    {
        if (m_initialized)
        {
            WSACleanup();
        }
    }


    UdpTestClient::~UdpTestClient()
    {
        Close();
    }

    void UdpTestClient::Close()
    {
        // Stop the receiver before closing the socket it is selecting on.
        if (m_receiverThread.joinable())
        {
            m_receiverThread.request_stop();
            m_receiverThread.join();
        }

        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        {
            std::lock_guard<std::mutex> lock(m_queueLock);
            m_queue.clear();
        }

        m_localPort = 0;
    }

    bool UdpTestClient::Open(HostEndpointAddress const& remote)
    {
        Close();

        if (!remote.IsValid())
        {
            return false;
        }

        auto portString = std::to_string(remote.Port);

        std::string hostUtf8;
        {
            auto required = WideCharToMultiByte(CP_UTF8, 0, remote.HostNameOrAddress.c_str(), -1, nullptr, 0, nullptr, nullptr);

            if (required <= 0)
            {
                return false;
            }

            hostUtf8.resize(static_cast<size_t>(required) - 1);
            WideCharToMultiByte(CP_UTF8, 0, remote.HostNameOrAddress.c_str(), -1, hostUtf8.data(), required, nullptr, nullptr);
        }

        addrinfo hints{ };
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        addrinfo* results{ nullptr };

        if (getaddrinfo(hostUtf8.c_str(), portString.c_str(), &hints, &results) != 0 || results == nullptr)
        {
            return false;
        }

        bool opened{ false };

        for (auto entry = results; entry != nullptr; entry = entry->ai_next)
        {
            auto candidate = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);

            if (candidate == INVALID_SOCKET)
            {
                continue;
            }

            // bind an ephemeral local port on the matching family
            sockaddr_storage local{ };
            int localLength{ 0 };

            if (entry->ai_family == AF_INET)
            {
                auto v4 = reinterpret_cast<sockaddr_in*>(&local);
                v4->sin_family = AF_INET;
                v4->sin_addr.s_addr = INADDR_ANY;
                v4->sin_port = 0;
                localLength = sizeof(sockaddr_in);
            }
            else
            {
                auto v6 = reinterpret_cast<sockaddr_in6*>(&local);
                v6->sin6_family = AF_INET6;
                v6->sin6_addr = in6addr_any;
                v6->sin6_port = 0;
                localLength = sizeof(sockaddr_in6);
            }

            if (bind(candidate, reinterpret_cast<sockaddr*>(&local), localLength) != 0)
            {
                closesocket(candidate);

                continue;
            }

            memcpy(&m_remoteAddress, entry->ai_addr, entry->ai_addrlen);
            m_remoteAddressLength = static_cast<int>(entry->ai_addrlen);
            m_socket = candidate;

            sockaddr_storage bound{ };
            int boundLength = sizeof(bound);

            if (getsockname(m_socket, reinterpret_cast<sockaddr*>(&bound), &boundLength) == 0)
            {
                if (bound.ss_family == AF_INET)
                {
                    m_localPort = ntohs(reinterpret_cast<sockaddr_in*>(&bound)->sin_port);
                }
                else
                {
                    m_localPort = ntohs(reinterpret_cast<sockaddr_in6*>(&bound)->sin6_port);
                }
            }

            opened = true;

            break;
        }

        freeaddrinfo(results);

        if (opened)
        {
            m_receiverThread = std::jthread([this](std::stop_token stopToken) { ReceiverLoop(stopToken); });
        }

        return opened;
    }

    bool UdpTestClient::Send(std::vector<uint8_t> const& bytes)
    {
        if (m_socket == INVALID_SOCKET)
        {
            return false;
        }

        auto sent = sendto(
            m_socket,
            reinterpret_cast<char const*>(bytes.data()),
            static_cast<int>(bytes.size()),
            0,
            reinterpret_cast<sockaddr const*>(&m_remoteAddress),
            m_remoteAddressLength);

        return sent == static_cast<int>(bytes.size());
    }

    void UdpTestClient::ReceiverLoop(std::stop_token const stopToken)
    {
        while (!stopToken.stop_requested())
        {
            auto socket = m_socket;

            if (socket == INVALID_SOCKET) break;

            fd_set readSet{ };
            FD_ZERO(&readSet);
            FD_SET(socket, &readSet);

            timeval tv{ };
            tv.tv_usec = 100 * 1000;        // short, so a stop request is noticed promptly

            if (select(0, &readSet, nullptr, nullptr, &tv) <= 0) continue;

            // comfortably larger than the spec's 1400 byte recommendation so oversized
            // datagrams are still observable rather than silently clipped
            std::vector<uint8_t> buffer(4096);

            sockaddr_storage from{ };
            int fromLength = sizeof(from);

            auto received = recvfrom(
                socket,
                reinterpret_cast<char*>(buffer.data()),
                static_cast<int>(buffer.size()),
                0,
                reinterpret_cast<sockaddr*>(&from),
                &fromLength);

            if (received <= 0) continue;

            buffer.resize(static_cast<size_t>(received));

            ReceivedDatagram item{ };
            item.Parsed = ParsePacket(buffer.data(), buffer.size());
            item.Bytes = std::move(buffer);

            if (m_autoPingReply)
            {
                if (auto ping = item.Parsed.Find(CommandCode::Ping); ping != nullptr)
                {
                    PacketBuilder reply;
                    reply.StartPacket().AddPingReply(ping->GetPayloadUInt32(0));

                    Send(reply);
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_queueLock);
                m_queue.push_back(std::move(item));
            }

            m_queueSignal.notify_all();
        }
    }

    std::optional<UdpTestClient::ReceivedDatagram> UdpTestClient::PopReceived(
        std::chrono::milliseconds const timeout)
    {
        std::unique_lock<std::mutex> lock(m_queueLock);

        if (!m_queueSignal.wait_for(lock, timeout, [this]() { return !m_queue.empty(); }))
        {
            return std::nullopt;
        }

        auto item = std::move(m_queue.front());
        m_queue.pop_front();

        return item;
    }

    std::optional<std::vector<uint8_t>> UdpTestClient::Receive(std::chrono::milliseconds const timeout)
    {
        auto item = PopReceived(timeout);

        if (!item.has_value()) return std::nullopt;

        return item->Bytes;
    }

    std::optional<ParsedPacket> UdpTestClient::ReceivePacket(std::chrono::milliseconds const timeout)
    {
        auto item = PopReceived(timeout);

        if (!item.has_value())
        {
            return std::nullopt;
        }

        m_receivedPackets.push_back(item->Parsed);

        return item->Parsed;
    }

    std::optional<ParsedPacket> UdpTestClient::WaitForCommand(
        CommandCode const code,
        std::chrono::milliseconds const timeout)
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());

            if (remaining.count() <= 0)
            {
                break;
            }

            auto packet = ReceivePacket(remaining);

            if (!packet.has_value())
            {
                continue;
            }

            if (packet->Contains(code))
            {
                return packet;
            }
        }

        return std::nullopt;
    }

    void UdpTestClient::DrainPending(std::chrono::milliseconds const settleTime)
    {
        while (Receive(settleTime).has_value())
        {
            // keep draining until nothing arrives within the settle time
        }

        m_receivedPackets.clear();
    }
}
