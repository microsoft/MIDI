// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A remote RTP-MIDI device for the tests: the protocol engine on its own loopback port pair. The
// sockets here are separate from the transport's, so a socket defect in the transport cannot
// hide itself. Loopback only, so nothing reaches the network and no firewall rule is needed.

namespace RtpMidiTest
{
    inline uint64_t SecureRandom64()
    {
        uint64_t value{ 0 };

        if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&value), sizeof(value), BCRYPT_USE_SYSTEM_PREFERRED_RNG)) || value == 0)
        {
            value = static_cast<uint64_t>(GetTickCount64()) * 0x9E3779B97F4A7C15ull;
        }

        return value;
    }

    inline bool WaitFor(std::function<bool()> const& condition, uint32_t const milliseconds)
    {
        auto const end = GetTickCount64() + milliseconds;

        while (GetTickCount64() < end)
        {
            if (condition()) return true;
            Sleep(20);
        }

        return condition();
    }

    // The AppleMIDI session clock: 100 microsecond ticks from QueryPerformanceCounter
    class SessionClock
    {
    public:
        SessionClock()
        {
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            m_frequency = static_cast<uint64_t>(frequency.QuadPart);
        }

        uint64_t Now() const
        {
            LARGE_INTEGER counter{};
            QueryPerformanceCounter(&counter);
            auto const ticks = static_cast<uint64_t>(counter.QuadPart);

            return (ticks / m_frequency) * RtpMidi::SessionClockTicksPerSecond +
                ((ticks % m_frequency) * RtpMidi::SessionClockTicksPerSecond) / m_frequency;
        }

    private:
        uint64_t m_frequency{ 1 };
    };

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

    // always IPv6, because the sockets are dual-stack
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

    inline bool TryParseAddress(std::wstring const& text, uint16_t const port, RtpMidi::PeerAddress& address)
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

        in6_addr v6{};
        if (InetPtonW(AF_INET6, text.c_str(), &v6) == 1)
        {
            address.Family = 6;
            memcpy(address.Bytes.data(), &v6, 16);
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

        // ::1, or 127.0.0.1 with ipv4, which a remote sees as a different host from ::1
        bool Bind(uint16_t const port, bool const ipv4)
        {
            auto const socket = WSASocketW(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
            if (socket == INVALID_SOCKET) return false;

            m_socket = socket;

            DWORD off = 0;
            setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char const*>(&off), sizeof(off));

            DWORD on = 1;
            setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char const*>(&on), sizeof(on));

            // an ICMP port unreachable from an earlier send must not break the next receive
            BOOL reportReset = FALSE;
            DWORD returned = 0;
            WSAIoctl(socket, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &returned, nullptr, nullptr);

            sockaddr_in6 local{};
            local.sin6_family = AF_INET6;
            local.sin6_addr = in6addr_loopback;
            local.sin6_port = htons(port);

            if (ipv4)
            {
                local.sin6_addr = in6_addr{};
                local.sin6_addr.u.Byte[10] = 0xFF;
                local.sin6_addr.u.Byte[11] = 0xFF;
                local.sin6_addr.u.Byte[12] = 127;
                local.sin6_addr.u.Byte[15] = 1;
            }

            if (bind(socket, reinterpret_cast<sockaddr const*>(&local), sizeof(local)) == SOCKET_ERROR)
            {
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

    // AppleMIDI needs the data port to be the control port plus one
    class PortPair
    {
    public:
        bool Bind(uint16_t const searchFrom, uint16_t const searchTo, bool const ipv4)
        {
            for (uint32_t port = searchFrom; port + 1 <= searchTo; port += 2)
            {
                if (!m_control.Bind(static_cast<uint16_t>(port), ipv4)) continue;
                if (m_data.Bind(static_cast<uint16_t>(port + 1), ipv4)) return true;

                m_control.Close();
            }

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
        UdpSocket m_control;
        UdpSocket m_data;
    };

    class Peer : public RtpMidi::ISessionHost
    {
    public:
        Peer(std::string const& name, bool const acceptInvitations, bool const ipv4 = false) :
            m_ipv4(ipv4),
            m_session(MakeConfig(name, acceptInvitations), *this, SecureRandom64())
        {
        }

        ~Peer() { Stop(); }

        bool Start()
        {
            if (!m_ports.Bind(47600, 47998, m_ipv4)) return false;

            m_ports.Control().StartReceiving([this](RtpMidi::PeerAddress const& from, uint8_t const* data, size_t size)
            {
                auto lock = std::scoped_lock{ m_lock };
                m_session.OnDatagram(true, from, data, size, m_clock.Now());
            });

            m_ports.Data().StartReceiving([this](RtpMidi::PeerAddress const& from, uint8_t const* data, size_t size)
            {
                auto lock = std::scoped_lock{ m_lock };
                m_session.OnDatagram(false, from, data, size, m_clock.Now());
            });

            m_ticker = std::thread([this]()
            {
                while (!m_stopping)
                {
                    {
                        auto lock = std::scoped_lock{ m_lock };
                        m_session.Tick(m_clock.Now());
                    }

                    Sleep(5);
                }
            });

            return true;
        }

        // ends every connection with a goodbye first
        void Stop()
        {
            if (m_stopped.exchange(true)) return;

            {
                auto lock = std::scoped_lock{ m_lock };
                m_session.EndAll(m_clock.Now());
                m_session.Tick(m_clock.Now());
            }

            m_stopping = true;
            if (m_ticker.joinable()) m_ticker.join();

            m_ports.Close();
        }

        uint16_t ControlPort() { return m_ports.Control().Port(); }

        void Invite(uint16_t const port)
        {
            RtpMidi::PeerAddress target{};
            TryParseAddress(m_ipv4 ? L"127.0.0.1" : L"::1", port, target);

            auto lock = std::scoped_lock{ m_lock };
            m_session.Invite(target, m_clock.Now());
        }

        void Send(std::vector<uint8_t> const& bytes)
        {
            auto lock = std::scoped_lock{ m_lock };
            m_session.SendMidi(bytes.data(), bytes.size(), m_clock.Now());
        }

        // like a sender that schedules ahead: the RTP timestamp is later than the send
        void SendAhead(std::vector<uint8_t> const& bytes, uint64_t const aheadSessionTicks)
        {
            auto lock = std::scoped_lock{ m_lock };
            m_session.SendMidi(bytes.data(), bytes.size(), m_clock.Now() + aheadSessionTicks);
        }

        void EndAll()
        {
            auto lock = std::scoped_lock{ m_lock };
            m_session.EndAll(m_clock.Now());
        }

        size_t ConnectedCount()
        {
            auto lock = std::scoped_lock{ m_lock };
            return m_session.ConnectedCount();
        }

        bool AnyClockSync()
        {
            auto lock = std::scoped_lock{ m_lock };
            for (auto const& participant : m_session.Snapshot())
            {
                if (participant.HaveClockOffset) return true;
            }
            return false;
        }

        std::vector<uint8_t> Received() { auto lock = std::scoped_lock{ m_lock }; return m_received; }
        std::vector<RtpMidi::EndReason> Ended() { auto lock = std::scoped_lock{ m_lock }; return m_ended; }
        std::string LastRemoteName() { auto lock = std::scoped_lock{ m_lock }; return m_lastRemoteName; }

        // Loopback answers in well under one 100 microsecond tick, which would measure as no
        // latency at all. Delaying clock sync replies makes the round trip visible.
        void SetSyncDelay(DWORD const milliseconds) { m_syncDelayMilliseconds = milliseconds; }

        // ISessionHost, called with m_lock held
        void SendControl(RtpMidi::PeerAddress const& to, std::vector<uint8_t> const& datagram) override { m_ports.Control().Send(to, datagram); }

        void SendData(RtpMidi::PeerAddress const& to, std::vector<uint8_t> const& datagram) override
        {
            RtpMidi::AppleMidiCommand command{};

            if (m_syncDelayMilliseconds != 0 &&
                RtpMidi::TryGetAppleMidiCommand(datagram.data(), datagram.size(), command) &&
                command == RtpMidi::AppleMidiCommand::Synchronization)
            {
                Sleep(m_syncDelayMilliseconds);
            }

            m_ports.Data().Send(to, datagram);
        }

        void OnMidi(RtpMidi::Participant const&, uint64_t, int64_t, bool, std::vector<uint8_t> const& bytes) override
        {
            m_received.insert(m_received.end(), bytes.begin(), bytes.end());
        }

        void OnParticipantChanged(RtpMidi::Participant const& participant) override
        {
            if (participant.State == RtpMidi::ParticipantState::Connected) m_lastRemoteName = participant.RemoteName;
            if (participant.State == RtpMidi::ParticipantState::Ended) m_ended.push_back(participant.Reason);
        }

        void Log(std::string const&) override {}

    private:
        static RtpMidi::SessionConfig MakeConfig(std::string const& name, bool const acceptInvitations)
        {
            RtpMidi::SessionConfig config{};
            config.LocalName = name;
            config.Ssrc = static_cast<uint32_t>(SecureRandom64());
            config.AcceptInvitations = acceptInvitations;
            config.SendJournal = true;
            return config;
        }

        SessionClock m_clock;
        bool m_ipv4{ false };
        PortPair m_ports;
        std::mutex m_lock;
        RtpMidi::Session m_session;
        std::thread m_ticker;
        std::atomic<bool> m_stopping{ false };
        std::atomic<bool> m_stopped{ false };
        std::vector<uint8_t> m_received;
        std::vector<RtpMidi::EndReason> m_ended;
        std::string m_lastRemoteName;
        std::atomic<DWORD> m_syncDelayMilliseconds{ 0 };
    };
}
