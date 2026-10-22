// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "OscHub.h"
#include "OscAddress.h"
#include "OscTime.h"
#include "StringResources.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <mstcpip.h>
#include <iphlpapi.h>

#include <midi_timestamp.h>

// Again, now that winsock2.h is in: that is what makes WIL define unique_socket.
#include <wil/resource.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        // What one call hands the engine at most. A SysEx message larger than this goes in pieces,
        // each ending on a whole UMP.
        constexpr size_t DeliveryWords = 256;

        // Messages waiting to be sent, beyond which the rest are dropped and counted.
        constexpr size_t MaximumQueuedMessages = 16384;

        // A message this close to its time is sent now. Less than a UDP packet's trip on a LAN.
        constexpr uint64_t SendEarlyMicroseconds = 1000;

        // A received message that says to play further ahead than this plays on arrival.
        constexpr uint64_t LongestHoldMilliseconds = 2000;

        // How often names are looked up again: the send host, and the addresses messages are
        // taken from. Sooner after a failure.
        constexpr uint64_t ResolveEveryMilliseconds = 30000;
        constexpr uint64_t ResolveAfterFailureMilliseconds = 5000;

        // Entries added to "recently heard" each second at most, so a flood costs little.
        constexpr uint32_t HeardPerSecond = 20;

        // A pattern costs this many messages against the rate limit, because matching it takes
        // as long as many plain messages.
        constexpr uint32_t PatternCost = 64;

        // Under this, a packet with several messages fits in one Ethernet frame.
        constexpr size_t PreferredBundleBytes = 1400;

        constexpr uint32_t TestNoteMilliseconds = 350;

        uint8_t UmpWords(_In_ uint32_t firstWord) noexcept
        {
            switch (firstWord >> 28)
            {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7: return 1;
            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA: return 2;
            case 0xB: case 0xC: return 3;
            default: return 4;
            }
        }

        // IPv4 addresses arrive on an IPv6 socket as ::ffff:a.b.c.d. Compared, shown and sent to
        // as IPv4.
        sockaddr_storage Normalized(_In_ sockaddr_storage const& address) noexcept
        {
            if (address.ss_family == AF_INET6)
            {
                auto const& six = reinterpret_cast<sockaddr_in6 const&>(address);

                if (IN6_IS_ADDR_V4MAPPED(&six.sin6_addr))
                {
                    sockaddr_storage four{};
                    auto& v4 = reinterpret_cast<sockaddr_in&>(four);

                    v4.sin_family = AF_INET;
                    v4.sin_port = six.sin6_port;
                    std::memcpy(&v4.sin_addr, &six.sin6_addr.u.Byte[12], 4);

                    return four;
                }
            }

            return address;
        }

        // The same computer, whatever the port.
        bool SameHost(_In_ sockaddr_storage const& first, _In_ sockaddr_storage const& second) noexcept
        {
            if (first.ss_family != second.ss_family)
            {
                return false;
            }

            if (first.ss_family == AF_INET)
            {
                return reinterpret_cast<sockaddr_in const&>(first).sin_addr.s_addr ==
                    reinterpret_cast<sockaddr_in const&>(second).sin_addr.s_addr;
            }

            if (first.ss_family == AF_INET6)
            {
                return std::memcmp(&reinterpret_cast<sockaddr_in6 const&>(first).sin6_addr,
                    &reinterpret_cast<sockaddr_in6 const&>(second).sin6_addr, sizeof(IN6_ADDR)) == 0;
            }

            return false;
        }

        bool SameAddressAndPort(_In_ sockaddr_storage const& first, _In_ sockaddr_storage const& second) noexcept
        {
            if (!SameHost(first, second))
            {
                return false;
            }

            return first.ss_family == AF_INET
                ? reinterpret_cast<sockaddr_in const&>(first).sin_port == reinterpret_cast<sockaddr_in const&>(second).sin_port
                : reinterpret_cast<sockaddr_in6 const&>(first).sin6_port == reinterpret_cast<sockaddr_in6 const&>(second).sin6_port;
        }

        void SetPort(_Inout_ sockaddr_storage& address, _In_ uint16_t port) noexcept
        {
            if (address.ss_family == AF_INET)
            {
                reinterpret_cast<sockaddr_in&>(address).sin_port = ::htons(port);
            }
            else if (address.ss_family == AF_INET6)
            {
                reinterpret_cast<sockaddr_in6&>(address).sin6_port = ::htons(port);
            }
        }

        int AddressLength(_In_ sockaddr_storage const& address) noexcept
        {
            return address.ss_family == AF_INET ? static_cast<int>(sizeof(sockaddr_in)) : static_cast<int>(sizeof(sockaddr_in6));
        }

        std::wstring AddressText(_In_ sockaddr_storage const& address, _In_ bool withPort)
        {
            wchar_t text[INET6_ADDRSTRLEN]{};
            uint16_t port{ 0 };

            if (address.ss_family == AF_INET)
            {
                auto const& v4 = reinterpret_cast<sockaddr_in const&>(address);
                ::InetNtopW(AF_INET, &v4.sin_addr, text, ARRAYSIZE(text));
                port = ::ntohs(v4.sin_port);
            }
            else if (address.ss_family == AF_INET6)
            {
                auto const& v6 = reinterpret_cast<sockaddr_in6 const&>(address);
                ::InetNtopW(AF_INET6, &v6.sin6_addr, text, ARRAYSIZE(text));
                port = ::ntohs(v6.sin6_port);

                if (withPort)
                {
                    return std::format(L"[{}]:{}", text, port);
                }
            }

            return withPort ? std::format(L"{}:{}", text, port) : std::wstring{ text };
        }

        // Every address a name or a numeric address stands for. Blocks while a name is looked up.
        std::vector<sockaddr_storage> Resolve(_In_ std::wstring const& host, _In_ uint16_t port)
        {
            std::vector<sockaddr_storage> addresses{};

            if (host.empty())
            {
                return addresses;
            }

            ADDRINFOW hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;

            PADDRINFOW found{ nullptr };
            auto const service = std::to_wstring(port);

            if (::GetAddrInfoW(host.c_str(), service.c_str(), &hints, &found) != 0 || found == nullptr)
            {
                return addresses;
            }

            for (auto item = found; item != nullptr; item = item->ai_next)
            {
                if (item->ai_addr == nullptr || item->ai_addrlen > sizeof(sockaddr_storage) ||
                    (item->ai_family != AF_INET && item->ai_family != AF_INET6))
                {
                    continue;
                }

                sockaddr_storage address{};
                std::memcpy(&address, item->ai_addr, item->ai_addrlen);
                addresses.push_back(Normalized(address));
            }

            ::FreeAddrInfoW(found);

            // IPv4 first: most OSC apps listen on IPv4 only.
            std::stable_partition(addresses.begin(), addresses.end(), [](sockaddr_storage const& a) { return a.ss_family == AF_INET; });

            return addresses;
        }

        // What has a UDP port, by its program's file name, when Windows says.
        std::wstring PortOwner(_In_ uint16_t port)
        {
            DWORD processId{ 0 };

            for (auto const family : { AF_INET, AF_INET6 })
            {
                DWORD size{ 0 };

                if (::GetExtendedUdpTable(nullptr, &size, FALSE, family, UDP_TABLE_OWNER_PID, 0) != ERROR_INSUFFICIENT_BUFFER || size == 0)
                {
                    continue;
                }

                std::vector<uint8_t> buffer(size);

                if (::GetExtendedUdpTable(buffer.data(), &size, FALSE, family, UDP_TABLE_OWNER_PID, 0) != NO_ERROR)
                {
                    continue;
                }

                if (family == AF_INET)
                {
                    auto const table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID const*>(buffer.data());

                    for (DWORD i = 0; i < table->dwNumEntries && processId == 0; i++)
                    {
                        if (::ntohs(static_cast<u_short>(table->table[i].dwLocalPort)) == port)
                        {
                            processId = table->table[i].dwOwningPid;
                        }
                    }
                }
                else
                {
                    auto const table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID const*>(buffer.data());

                    for (DWORD i = 0; i < table->dwNumEntries && processId == 0; i++)
                    {
                        if (::ntohs(static_cast<u_short>(table->table[i].dwLocalPort)) == port)
                        {
                            processId = table->table[i].dwOwningPid;
                        }
                    }
                }

                if (processId != 0)
                {
                    break;
                }
            }

            if (processId == 0 || processId == ::GetCurrentProcessId())
            {
                return {};
            }

            wil::unique_handle process{ ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) };

            if (!process)
            {
                return {};
            }

            wchar_t path[MAX_PATH]{};
            DWORD length{ ARRAYSIZE(path) };

            if (!::QueryFullProcessImageNameW(process.get(), 0, path, &length))
            {
                return {};
            }

            return std::filesystem::path{ path }.filename().wstring();
        }

        // Windows reports an earlier send's "port unreachable" on the next receive. A receive
        // socket doesn't care.
        void IgnorePortUnreachable(_In_ SOCKET handle) noexcept
        {
            BOOL off{ FALSE };
            DWORD returned{ 0 };

            ::WSAIoctl(handle, SIO_UDP_CONNRESET, &off, sizeof(off), nullptr, 0, &returned, nullptr, nullptr);
        }

        int64_t FileTimeNow() noexcept
        {
            FILETIME now{};
            ::GetSystemTimePreciseAsFileTime(&now);

            return static_cast<int64_t>((static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime);
        }

        osc::ClockPair ReadClock() noexcept
        {
            osc::ClockPair clock{};

            clock.FileTime = static_cast<uint64_t>(FileTimeNow());
            clock.Ticks = internal::GetCurrentMidiTimestamp();
            clock.TicksPerSecond = internal::GetMidiTimestampFrequency();

            return clock;
        }

        wil::unique_handle CreateSendTimer() noexcept
        {
            wil::unique_handle timer{ ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS) };

            if (!timer)
            {
                timer.reset(::CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS));
            }

            return timer;
        }

        // A socket for sending to one address. Connected, so it binds to the one local address
        // that reaches it rather than to every network, which the firewall would ask about.
        wil::unique_socket ConnectedSocket(_In_ sockaddr_storage const& destination) noexcept
        {
            wil::unique_socket handle{ ::WSASocketW(destination.ss_family, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT) };

            if (!handle)
            {
                return {};
            }

            IgnorePortUnreachable(handle.get());

            if (::connect(handle.get(), reinterpret_cast<sockaddr const*>(&destination), AddressLength(destination)) == SOCKET_ERROR)
            {
                return {};
            }

            return handle;
        }

        // Sends what the translator made, as packets: one message each, or bundles.
        bool SendPackets(
            _In_ SOCKET handle,
            _In_ std::vector<osc::MessageBuilder> const& messages,
            _In_ bool bundle,
            _In_ uint64_t timeTag,
            _Inout_ uint64_t& packetsSent) noexcept
        {
            try
            {
                auto ok = true;

                auto const send = [&](std::vector<uint8_t> const& packet)
                    {
                        if (packet.size() > osc::MaximumPacketBytes ||
                            ::send(handle, reinterpret_cast<char const*>(packet.data()), static_cast<int>(packet.size()), 0) == SOCKET_ERROR)
                        {
                            ok = false;
                            return;
                        }

                        packetsSent++;
                    };

                if (!bundle)
                {
                    for (auto const& message : messages)
                    {
                        send(osc::MakePacket(message));
                    }

                    return ok;
                }

                size_t start{ 0 };

                while (start < messages.size())
                {
                    // 20 bytes of bundle header and element size, then the messages that fit.
                    size_t bytes{ 20 + messages[start].Size() };
                    auto end = start + 1;

                    while (end < messages.size() && bytes + 4 + messages[end].Size() <= PreferredBundleBytes)
                    {
                        bytes += 4 + messages[end].Size();
                        end++;
                    }

                    send(osc::MakeBundle(timeTag, std::span<osc::MessageBuilder const>{ messages.data() + start, end - start }));
                    start = end;
                }

                return ok;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    // The sockets on one local port, and a thread waiting on each.
    struct OscHub::Port
    {
        uint16_t Number{ 0 };
        bool EveryNetwork{ false };
        OscListenState State{ OscListenState::Failed };
        std::wstring Owner{};

        std::vector<wil::unique_socket> Sockets{};
        std::vector<std::thread> Threads{};
        std::atomic<bool> Closing{ false };

        // Replaced whole when the connections on the port change, so a receive thread works on
        // the list it took.
        std::atomic<std::shared_ptr<std::vector<winrt::com_ptr<Connection>> const>> Listeners{};

        Port() noexcept = default;
        Port(Port const&) = delete;
        Port& operator=(Port const&) = delete;

        ~Port() noexcept
        {
            Close();
        }

        bool Open(_In_ uint16_t number, _In_ bool everyNetwork) noexcept;
        void Close() noexcept;

    private:
        bool Bind(_In_ int family, _In_ bool everyNetwork, _Out_ int& error) noexcept;
        void ReceiveLoop(_In_ SOCKET handle) noexcept;

        // Set to stop every receive thread at once.
        wil::unique_event_nothrow m_stop{};
    };
}

namespace midipatchbay
{
    // One OSC connection, as the routing engine sees an endpoint.
    class OscHub::Connection : public winrt::implements<OscHub::Connection, IMidiEndpointConnectionRaw>
    {
    public:
        explicit Connection(_In_ RouteOscConnection const& planned)
        {
            m_deviceId = planned.DeviceId;
            Update(planned);
        }

        ~Connection() noexcept
        {
            Stop();
        }

        std::wstring const& DeviceId() const noexcept
        {
            return m_deviceId;
        }

        OscConnectionSettings Settings() const
        {
            auto const config = m_config.load();
            return config == nullptr ? OscConnectionSettings{} : config->Settings;
        }

        // Takes effect on the next packet. The worker looks names up again and starts SysEx
        // collection afresh.
        void Update(_In_ RouteOscConnection const& planned)
        {
            auto config = std::make_shared<Config>();
            config->Name = planned.Name;
            config->Settings = planned.Settings;
            config->Version = ++m_versions;

            m_accepted.store(nullptr);
            m_config.store(std::move(config));

            if (m_wake)
            {
                ::SetEvent(m_wake.get());
            }
        }

        bool Start() noexcept
        {
            try
            {
                m_wake.reset(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
                m_timer = CreateSendTimer();

                if (!m_wake || !m_timer)
                {
                    return false;
                }

                m_stop = false;
                m_worker = std::thread([this]() { Work(); });

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        // Nothing is sent or delivered after this.
        void Stop() noexcept
        {
            m_closed = true;
            m_stop = true;

            if (m_wake)
            {
                ::SetEvent(m_wake.get());
            }

            if (m_worker.joinable())
            {
                m_worker.join();
            }

            std::scoped_lock guard{ m_dispatch };
            m_callback = nullptr;
        }

        void SetListening(_In_ OscListenState state, _In_ bool heldByFirewall, _In_ uint16_t port, _In_ std::wstring const& owner)
        {
            std::scoped_lock guard{ m_statusLock };

            m_listening = state;
            m_heldByFirewall = heldByFirewall;
            m_port = port;
            m_portOwner = owner;
        }

        // On a port's receive thread.
        void Receive(
            _In_reads_(size) uint8_t const* packet,
            _In_ size_t size,
            _In_ sockaddr_storage const& from,
            _In_ uint64_t arrival,
            _In_ osc::ClockPair const& clock) noexcept
        {
            auto const config = m_config.load();

            if (config == nullptr || m_closed)
            {
                return;
            }

            if (!Accepts(*config, from))
            {
                Reader reader{ *this, *config, from, arrival, clock, false };
                osc::ReadPacket(packet, size, reader);
                return;
            }

            {
                std::scoped_lock guard{ m_senderLock };
                m_lastSender = from;
                m_hasLastSender = true;
            }

            std::scoped_lock guard{ m_dispatch };

            // A fresh second of the rate limit.
            if (arrival - m_rateWindowStart >= clock.TicksPerSecond)
            {
                m_rateWindowStart = arrival;
                m_rateUsed = 0;
            }

            Reader reader{ *this, *config, from, arrival, clock, true };

            if (osc::ReadPacket(packet, size, reader) != osc::ReadProblem::None && reader.Messages == 0)
            {
                RecordUnreadable(from, clock);
            }
        }

        OscConnectionStatus Status() const
        {
            OscConnectionStatus status{};

            {
                std::scoped_lock guard{ m_statusLock };

                status.Listening = m_listening;
                status.HeldByFirewall = m_heldByFirewall;
                status.Port = m_port;
                status.PortOwner = m_portOwner;
                status.HostNotFound = m_hostNotFound;
                status.SendingTo = m_sendingTo;
                status.LastHeardWhen = m_lastHeardWhen;
                status.LastHeardFrom = m_lastHeardFrom;
                status.RecentlyHeard.assign(m_heard.begin(), m_heard.end());
            }

            if (auto const config = m_config.load(); config != nullptr)
            {
                status.Sends = OscConnectionSends(config->Settings);
            }

            status.OscReceived = m_oscReceived;
            status.MidiReceived = m_midiReceived;
            status.Unreadable = m_unreadable;
            status.NoRowMatched = m_noRowMatched;
            status.NotAccepted = m_notAccepted;
            status.RateLimited = m_rateLimited;
            status.TooFarAhead = m_tooFarAhead;
            status.TooManyFromPattern = m_tooManyFromPattern;
            status.MidiSent = m_midiSent;
            status.OscSent = m_oscSent;
            status.NotSendable = m_notSendable;
            status.LeftOut = m_leftOut;
            status.NoRow = m_noRow;
            status.SendDropped = m_sendDropped;
            status.SendFailures = m_sendFailures;

            return status;
        }

        void ClearRecentlyHeard() noexcept
        {
            std::scoped_lock guard{ m_statusLock };
            m_heard.clear();
        }

        // IMidiEndpointConnectionRaw

        STDMETHOD_(UINT32, GetSupportedMaxMidiWordsPerTransmission)() override
        {
            return static_cast<UINT32>(DeliveryWords);
        }

        STDMETHOD_(BOOL, ValidateBufferHasOnlyCompleteUmps)(_In_ UINT32 wordCount, _In_ UINT32 const* messages) override
        {
            if (messages == nullptr)
            {
                return FALSE;
            }

            UINT32 position{ 0 };

            while (position < wordCount)
            {
                position += UmpWords(messages[position]);
            }

            return position == wordCount ? TRUE : FALSE;
        }

        // On whichever thread the engine sends from, which may be a device's receive callback:
        // queues and returns.
        STDMETHOD(SendMidiMessagesRaw)(_In_ UINT64 timestamp, _In_ UINT32 wordCount, _In_ UINT32 const* messages) override
        {
            if (messages == nullptr || wordCount == 0)
            {
                return E_INVALIDARG;
            }

            auto const config = m_config.load();

            if (config == nullptr || m_closed || !OscConnectionSends(config->Settings))
            {
                m_sendDropped++;
                return S_OK;
            }

            try
            {
                // 0 is "now" to the MIDI service, and to the queue.
                if (timestamp == 0)
                {
                    timestamp = internal::GetCurrentMidiTimestamp();
                }

                {
                    std::scoped_lock guard{ m_queueLock };

                    UINT32 position{ 0 };

                    while (position < wordCount)
                    {
                        auto const length = UmpWords(messages[position]);

                        if (position + length > wordCount)
                        {
                            break;
                        }

                        if (m_queue.size() >= MaximumQueuedMessages)
                        {
                            m_sendDropped++;
                        }
                        else
                        {
                            Entry entry{};
                            entry.Timestamp = timestamp;
                            entry.WordCount = length;
                            std::copy_n(messages + position, length, entry.Words.begin());

                            // Almost always in order, so this is almost always the end.
                            auto place = m_queue.end();

                            while (place != m_queue.begin() && std::prev(place)->Timestamp > timestamp)
                            {
                                --place;
                            }

                            m_queue.insert(place, entry);
                        }

                        position += length;
                    }
                }

                ::SetEvent(m_wake.get());

                return S_OK;
            }
            catch (...)
            {
                m_sendDropped++;
                return S_OK;
            }
        }

        STDMETHOD(SetMessagesReceivedCallback)(_In_ IMidiEndpointConnectionMessagesReceivedCallback* callback) override
        {
            if (callback == nullptr)
            {
                return E_INVALIDARG;
            }

            std::scoped_lock guard{ m_dispatch };
            m_callback.copy_from(callback);

            return S_OK;
        }

        // Waits for a delivery in progress, so the engine can let its hub go once this returns.
        STDMETHOD(RemoveMessagesReceivedCallback)() override
        {
            std::scoped_lock guard{ m_dispatch };
            m_callback = nullptr;

            return S_OK;
        }

    private:
        struct Config
        {
            std::wstring Name{};
            OscConnectionSettings Settings{};
            uint64_t Version{ 0 };
        };

        struct Entry
        {
            uint64_t Timestamp{ 0 };
            std::array<uint32_t, 4> Words{};
            uint8_t WordCount{ 0 };
        };

        // Hands what a received message became to the engine, in pieces that end on whole UMPs.
        class Delivery final : public osc::UmpReceiver
        {
        public:
            Delivery(_In_ Connection& owner, _In_ uint64_t timestamp) noexcept :
                m_owner{ owner }, m_timestamp{ timestamp }
            {
            }

            void OnUmp(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept override
            {
                if (m_used + wordCount > m_words.size())
                {
                    Flush();
                }

                std::copy_n(words, wordCount, m_words.begin() + m_used);
                m_used += wordCount;
            }

            // Needs the owner's dispatch lock.
            void Flush() noexcept
            {
                if (m_used > 0 && m_owner.m_callback != nullptr)
                {
                    LOG_IF_FAILED(m_owner.m_callback->MessagesReceived(GUID{}, GUID{}, m_timestamp, static_cast<UINT32>(m_used), m_words.data()));
                }

                m_used = 0;
            }

        private:
            Connection& m_owner;
            uint64_t m_timestamp{ 0 };
            std::array<uint32_t, DeliveryWords> m_words{};
            size_t m_used{ 0 };
        };

        class Reader final : public osc::MessageVisitor
        {
        public:
            Reader(
                _In_ Connection& owner,
                _In_ Config const& config,
                _In_ sockaddr_storage const& from,
                _In_ uint64_t arrival,
                _In_ osc::ClockPair const& clock,
                _In_ bool accepted) noexcept :
                m_owner{ owner }, m_config{ config }, m_from{ from }, m_arrival{ arrival }, m_clock{ clock }, m_accepted{ accepted }
            {
            }

            size_t Messages{ 0 };

            void OnMessage(_In_ osc::MessageView const& message, _In_ uint64_t timeTag) noexcept override
            {
                Messages++;

                if (!m_accepted)
                {
                    // Counted as received, so "kept out" reads as part of what arrived.
                    m_owner.m_oscReceived++;
                    m_owner.m_notAccepted++;
                    m_owner.RecordHeard(message, m_from, m_clock, false, false);
                    return;
                }

                m_owner.OnMessage(m_config, message, timeTag, m_from, m_arrival, m_clock);
            }

            void OnUnreadable(_In_ osc::ReadProblem) noexcept override
            {
                if (m_accepted)
                {
                    m_owner.m_unreadable++;
                }
            }

        private:
            Connection& m_owner;
            Config const& m_config;
            sockaddr_storage const& m_from;
            uint64_t m_arrival{ 0 };
            osc::ClockPair const& m_clock;
            bool m_accepted{ true };
        };

        bool Accepts(_In_ Config const& config, _In_ sockaddr_storage const& from) const noexcept
        {
            auto const& settings = config.Settings;

            if (settings.AcceptFrom == OscAcceptFrom::Anyone ||
                (settings.AcceptFrom == OscAcceptFrom::SendHost && settings.SendHost.empty()) ||
                (settings.AcceptFrom == OscAcceptFrom::List && settings.AcceptAddresses.empty()))
            {
                return true;
            }

            // Until the names are looked up, nobody.
            auto const accepted = m_accepted.load();

            if (accepted == nullptr)
            {
                return false;
            }

            return std::any_of(accepted->begin(), accepted->end(), [&](sockaddr_storage const& address) { return SameHost(address, from); });
        }

        // Needs m_dispatch.
        uint64_t TimestampFor(_In_ Config const& config, _In_ uint64_t timeTag, _In_ uint64_t arrival, _In_ osc::ClockPair const& clock) noexcept
        {
            auto const& settings = config.Settings;

            if (timeTag <= osc::Immediately || settings.ReceivedTimeTags == OscReceivedTimeTags::PlayOnArrival)
            {
                return arrival;
            }

            uint64_t playAt{ arrival };

            if (settings.ReceivedTimeTags == OscReceivedTimeTags::UseSenderTime)
            {
                playAt = osc::TicksFromTimeTag(timeTag, clock);
            }
            else
            {
                auto const delay = clock.TicksPerSecond * settings.SpacingDelayMilliseconds / 1000;
                playAt = m_spacing.PlayAt(timeTag, arrival, delay, clock.TicksPerSecond);
            }

            if (playAt < arrival)
            {
                return arrival;
            }

            if (playAt - arrival > clock.TicksPerSecond * LongestHoldMilliseconds / 1000)
            {
                m_tooFarAhead++;
                return arrival;
            }

            return playAt;
        }

        // Needs m_dispatch.
        void OnMessage(
            _In_ Config const& config,
            _In_ osc::MessageView const& message,
            _In_ uint64_t timeTag,
            _In_ sockaddr_storage const& from,
            _In_ uint64_t arrival,
            _In_ osc::ClockPair const& clock) noexcept
        {
            m_oscReceived++;

            auto const cost = osc::HasPatternCharacters(message.Address) ? PatternCost : 1u;

            if (m_rateUsed + cost > config.Settings.ReceiveRateLimit)
            {
                m_rateLimited++;
                return;
            }

            m_rateUsed += cost;

            Delivery delivery{ *this, TimestampFor(config, timeTag, arrival, clock) };
            auto const outcome = osc::TranslateInbound(config.Settings.Rows, message, delivery);
            delivery.Flush();

            m_midiReceived += outcome.Messages;

            switch (outcome.Result)
            {
            case osc::InboundResult::NoRowMatched: m_noRowMatched++; break;
            case osc::InboundResult::Unreadable: m_unreadable++; break;
            case osc::InboundResult::TooManyMessages: m_tooManyFromPattern++; break;
            default: break;
            }

            RecordHeard(message, from, clock, outcome.Result != osc::InboundResult::NoRowMatched, true);
        }

        // Allocates, so it's limited to a few each second.
        void RecordHeard(
            _In_ osc::MessageView const& message,
            _In_ sockaddr_storage const& from,
            _In_ osc::ClockPair const& clock,
            _In_ bool matched,
            _In_ bool accepted) noexcept
        {
            try
            {
                auto const fromText = AddressText(from, false);

                std::scoped_lock guard{ m_statusLock };

                m_lastHeardWhen = static_cast<int64_t>(clock.FileTime);
                m_lastHeardFrom = fromText;

                if (clock.Ticks - m_heardWindowStart >= clock.TicksPerSecond)
                {
                    m_heardWindowStart = clock.Ticks;
                    m_heardThisWindow = 0;
                }

                if (m_heardThisWindow >= HeardPerSecond)
                {
                    return;
                }

                m_heardThisWindow++;

                OscHeardMessage heard{};

                for (auto const c : message.Address.substr(0, osc::MaximumAddressLength))
                {
                    heard.Address += c > 0x20 && c < 0x7F ? static_cast<wchar_t>(c) : L'?';
                }

                heard.Arguments = osc::ArgumentsText(message);
                heard.From = fromText;
                heard.When = static_cast<int64_t>(clock.FileTime);
                heard.Matched = matched;
                heard.Accepted = accepted;

                if (!matched)
                {
                    heard.Suggestion = osc::SuggestRow(message, 0);

                    // An address that isn't any format's can still have a rule.
                    if (!heard.Suggestion.has_value())
                    {
                        auto const config = m_config.load();

                        heard.RuleSuggestion = osc::SuggestRule(message, config != nullptr
                            ? std::span<OscRow const>{ config->Settings.Rows }
                            : std::span<OscRow const>{});
                    }
                }

                m_heard.push_front(std::move(heard));

                while (m_heard.size() > MaximumRecentlyHeard)
                {
                    m_heard.pop_back();
                }
            }
            catch (...)
            {
            }
        }

        void RecordUnreadable(_In_ sockaddr_storage const& from, _In_ osc::ClockPair const& clock) noexcept
        {
            try
            {
                auto const fromText = AddressText(from, false);

                std::scoped_lock guard{ m_statusLock };

                m_lastHeardWhen = static_cast<int64_t>(clock.FileTime);
                m_lastHeardFrom = fromText;
            }
            catch (...)
            {
            }
        }

        // Looks up the send host and the addresses messages are taken from. Blocks while a name is
        // looked up. On the worker.
        void Resolve(_In_ Config const& config)
        {
            auto const& settings = config.Settings;

            auto hosts = Resolve(settings.SendHost, settings.SendPort);

            std::shared_ptr<std::vector<sockaddr_storage>> accepted{};

            if (settings.AcceptFrom == OscAcceptFrom::SendHost)
            {
                accepted = std::make_shared<std::vector<sockaddr_storage>>(hosts);
            }
            else if (settings.AcceptFrom == OscAcceptFrom::List)
            {
                accepted = std::make_shared<std::vector<sockaddr_storage>>();

                for (auto const& name : settings.AcceptAddresses)
                {
                    auto const found = Resolve(name, 0);
                    accepted->insert(accepted->end(), found.begin(), found.end());
                }
            }

            // Only if nothing changed meanwhile, so an old answer never replaces a new setting.
            if (m_config.load().get() == &config)
            {
                m_accepted.store(std::move(accepted));
            }

            m_hostAddresses = std::move(hosts);
            m_hostResolved = !settings.SendHost.empty() && !m_hostAddresses.empty();

            std::scoped_lock guard{ m_statusLock };
            m_hostNotFound = !settings.SendHost.empty() && m_hostAddresses.empty();
        }

        std::vector<sockaddr_storage> Resolve(_In_ std::wstring const& host, _In_ uint16_t port)
        {
            try
            {
                return ::midipatchbay::Resolve(host, port);
            }
            catch (...)
            {
                return {};
            }
        }

        // Where to send now: whoever sent last, when asked and known, or the send host.
        std::optional<sockaddr_storage> Destination(_In_ Config const& config)
        {
            auto const& settings = config.Settings;

            if (settings.SendToLastSender)
            {
                std::scoped_lock guard{ m_senderLock };

                if (m_hasLastSender)
                {
                    auto address = Normalized(m_lastSender);
                    SetPort(address, settings.SendPort);
                    return address;
                }
            }

            if (!m_hostAddresses.empty())
            {
                return m_hostAddresses.front();
            }

            return std::nullopt;
        }

        // On the worker. Sends what's due, and says when the next one is due: 0 for nothing.
        uint64_t SendDue(_In_ Config const& config, _In_ uint64_t now, _In_ uint64_t ticksPerSecond)
        {
            auto const& settings = config.Settings;
            auto const margin = ticksPerSecond * SendEarlyMicroseconds / 1'000'000;

            std::vector<Entry> due{};
            uint64_t next{ 0 };

            {
                std::scoped_lock guard{ m_queueLock };

                if (settings.SendTiming == OscSendTiming::AtItsTime)
                {
                    while (!m_queue.empty() && m_queue.front().Timestamp <= now + margin)
                    {
                        due.push_back(m_queue.front());
                        m_queue.pop_front();
                    }

                    next = m_queue.empty() ? 0 : m_queue.front().Timestamp;
                }
                else
                {
                    due.assign(m_queue.begin(), m_queue.end());
                    m_queue.clear();
                }
            }

            if (due.empty())
            {
                return next;
            }

            auto const destination = Destination(config);

            if (destination.has_value() && (!m_socket || !SameAddressAndPort(*destination, m_socketDestination)))
            {
                m_socket = ConnectedSocket(*destination);
                m_socketDestination = *destination;

                std::scoped_lock guard{ m_statusLock };
                m_sendingTo = m_socket ? AddressText(*destination, true) : std::wstring{};
            }

            auto const clock = ReadClock();
            std::vector<osc::MessageBuilder> out{};

            size_t start{ 0 };

            while (start < due.size())
            {
                // A run of messages that play at the same moment.
                auto end = start + 1;

                while (end < due.size() && due[end].Timestamp == due[start].Timestamp)
                {
                    end++;
                }

                out.clear();
                uint64_t translated{ 0 };

                for (auto i = start; i < end; i++)
                {
                    switch (m_translator.Translate(settings.Rows, due[i].Words.data(), due[i].WordCount, out))
                    {
                    case osc::OutboundResult::Sent: translated++; break;
                    case osc::OutboundResult::NoRow: m_noRow++; break;
                    case osc::OutboundResult::LeftOut: m_leftOut++; break;
                    case osc::OutboundResult::Waiting: break;
                    default: m_notSendable++; break;
                    }
                }

                if (!out.empty())
                {
                    if (!destination.has_value() || !m_socket)
                    {
                        m_sendDropped += translated;
                    }
                    else
                    {
                        auto const later = settings.SendTiming == OscSendTiming::EarlyWithTimeTags &&
                            due[start].Timestamp > clock.Ticks + margin;

                        uint64_t packets{ 0 };

                        if (SendPackets(m_socket.get(), out, later || settings.BundleTogether,
                            later ? osc::TimeTagFromTicks(due[start].Timestamp, clock) : osc::Immediately, packets))
                        {
                            m_midiSent += translated;
                        }
                        else
                        {
                            m_sendFailures++;
                        }

                        m_oscSent += packets;
                    }
                }

                start = end;
            }

            return next;
        }

        void Work() noexcept
        {
            ::SetThreadDescription(::GetCurrentThread(), L"MIDI Patchbay OSC sender");

            auto const ticksPerSecond = internal::GetMidiTimestampFrequency();
            uint64_t seenVersion{ 0 };
            uint64_t nextResolve{ 0 };

            while (!m_stop)
            {
                try
                {
                    auto const config = m_config.load();

                    if (config == nullptr)
                    {
                        break;
                    }

                    auto now = internal::GetCurrentMidiTimestamp();

                    if (config->Version != seenVersion)
                    {
                        seenVersion = config->Version;
                        m_translator.Reset();
                        m_socket.reset();
                        nextResolve = 0;
                    }

                    if (now >= nextResolve)
                    {
                        Resolve(*config);

                        auto const failed = !config->Settings.SendHost.empty() && !m_hostResolved;
                        now = internal::GetCurrentMidiTimestamp();
                        nextResolve = now + ticksPerSecond * (failed ? ResolveAfterFailureMilliseconds : ResolveEveryMilliseconds) / 1000;
                    }

                    auto const next = SendDue(*config, now, ticksPerSecond);

                    DWORD waitMilliseconds = static_cast<DWORD>((std::min)(uint64_t{ 1000 },
                        nextResolve > now ? (nextResolve - now) * 1000 / ticksPerSecond : 0));

                    if (next != 0)
                    {
                        now = internal::GetCurrentMidiTimestamp();

                        // Woken at the time itself. The margin only lets a wake that comes a
                        // little early send rather than sleep again.
                        auto const ticks = next > now ? next - now : 0;

                        LARGE_INTEGER dueTime{};
                        dueTime.QuadPart = -static_cast<LONGLONG>((std::max)(ticks * 10'000'000ull / ticksPerSecond, 1ull));
                        ::SetWaitableTimer(m_timer.get(), &dueTime, 0, nullptr, nullptr, FALSE);
                    }

                    HANDLE const handles[2]{ m_wake.get(), m_timer.get() };
                    ::WaitForMultipleObjects(2, handles, FALSE, waitMilliseconds);
                }
                catch (...)
                {
                    // Something out of memory: wait a moment rather than spin.
                    ::Sleep(50);
                }
            }

            ::CancelWaitableTimer(m_timer.get());
        }

        std::wstring m_deviceId{};
        std::atomic<uint64_t> m_versions{ 0 };
        std::atomic<std::shared_ptr<Config const>> m_config{};

        // The addresses messages are taken from, once looked up. Null takes nothing.
        std::atomic<std::shared_ptr<std::vector<sockaddr_storage> const>> m_accepted{};

        std::atomic<bool> m_closed{ false };
        std::atomic<bool> m_stop{ false };

        // Held while a received message goes to the engine, and while the callback changes.
        std::mutex m_dispatch{};
        winrt::com_ptr<IMidiEndpointConnectionMessagesReceivedCallback> m_callback{};
        osc::SpacingKeeper m_spacing{};
        uint64_t m_rateWindowStart{ 0 };
        uint32_t m_rateUsed{ 0 };

        std::mutex m_senderLock{};
        sockaddr_storage m_lastSender{};
        bool m_hasLastSender{ false };

        std::mutex m_queueLock{};
        std::deque<Entry> m_queue{};

        // The worker's own.
        std::thread m_worker{};
        wil::unique_event_nothrow m_wake{};
        wil::unique_handle m_timer{};
        osc::OutboundTranslator m_translator{};
        std::vector<sockaddr_storage> m_hostAddresses{};
        bool m_hostResolved{ false };
        wil::unique_socket m_socket{};
        sockaddr_storage m_socketDestination{};

        mutable std::mutex m_statusLock{};
        OscListenState m_listening{ OscListenState::NotReceiving };
        bool m_heldByFirewall{ false };
        uint16_t m_port{ 0 };
        std::wstring m_portOwner{};
        bool m_hostNotFound{ false };
        std::wstring m_sendingTo{};
        int64_t m_lastHeardWhen{ 0 };
        std::wstring m_lastHeardFrom{};
        std::deque<OscHeardMessage> m_heard{};
        uint64_t m_heardWindowStart{ 0 };
        uint32_t m_heardThisWindow{ 0 };

        std::atomic<uint64_t> m_oscReceived{ 0 };
        std::atomic<uint64_t> m_midiReceived{ 0 };
        std::atomic<uint64_t> m_unreadable{ 0 };
        std::atomic<uint64_t> m_noRowMatched{ 0 };
        std::atomic<uint64_t> m_notAccepted{ 0 };
        std::atomic<uint64_t> m_rateLimited{ 0 };
        std::atomic<uint64_t> m_tooFarAhead{ 0 };
        std::atomic<uint64_t> m_tooManyFromPattern{ 0 };
        std::atomic<uint64_t> m_midiSent{ 0 };
        std::atomic<uint64_t> m_oscSent{ 0 };
        std::atomic<uint64_t> m_notSendable{ 0 };
        std::atomic<uint64_t> m_leftOut{ 0 };
        std::atomic<uint64_t> m_noRow{ 0 };
        std::atomic<uint64_t> m_sendDropped{ 0 };
        std::atomic<uint64_t> m_sendFailures{ 0 };
    };
}

namespace midipatchbay
{
    _Use_decl_annotations_
    bool OscHub::Port::Bind(int family, bool everyNetwork, int& error) noexcept
    {
        error = 0;

        wil::unique_socket handle{ ::WSASocketW(family, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT) };

        if (!handle)
        {
            error = ::WSAGetLastError();
            return false;
        }

        // Nobody else can take the port while it's ours, and we can't take one that's someone else's.
        BOOL exclusive{ TRUE };
        ::setsockopt(handle.get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char const*>(&exclusive), sizeof(exclusive));

        // Room for a burst while the receive thread is busy.
        int receiveBuffer{ 1 << 20 };
        ::setsockopt(handle.get(), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char const*>(&receiveBuffer), sizeof(receiveBuffer));

        IgnorePortUnreachable(handle.get());

        sockaddr_storage address{};

        if (family == AF_INET6)
        {
            // On every network, one socket takes IPv4 and IPv6 alike.
            DWORD v6Only{ everyNetwork ? 0u : 1u };
            ::setsockopt(handle.get(), IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char const*>(&v6Only), sizeof(v6Only));

            auto& v6 = reinterpret_cast<sockaddr_in6&>(address);
            v6.sin6_family = AF_INET6;
            v6.sin6_port = ::htons(Number);
            v6.sin6_addr = everyNetwork ? in6addr_any : in6addr_loopback;
        }
        else
        {
            auto& v4 = reinterpret_cast<sockaddr_in&>(address);
            v4.sin_family = AF_INET;
            v4.sin_port = ::htons(Number);
            v4.sin_addr.s_addr = ::htonl(everyNetwork ? INADDR_ANY : INADDR_LOOPBACK);
        }

        if (::bind(handle.get(), reinterpret_cast<sockaddr const*>(&address), AddressLength(address)) == SOCKET_ERROR)
        {
            error = ::WSAGetLastError();
            return false;
        }

        Sockets.push_back(std::move(handle));

        return true;
    }

    _Use_decl_annotations_
    bool OscHub::Port::Open(uint16_t number, bool everyNetwork) noexcept
    {
        try
        {
            Number = number;
            EveryNetwork = everyNetwork;
            Closing = false;

            m_stop.reset(::CreateEventW(nullptr, TRUE, FALSE, nullptr));

            if (!m_stop)
            {
                State = OscListenState::Failed;
                return false;
            }

            int error{ 0 };
            auto opened = false;

            if (everyNetwork)
            {
                opened = Bind(AF_INET6, true, error);

                // A PC without IPv6 still has IPv4. A port in use is in use either way.
                if (!opened && error != WSAEADDRINUSE && error != WSAEACCES)
                {
                    opened = Bind(AF_INET, true, error);
                }
            }
            else
            {
                opened = Bind(AF_INET, false, error);

                if (opened)
                {
                    // IPv6's ::1 too, for apps that send to "localhost" that way. Not having it
                    // isn't a failure.
                    int ignored{ 0 };
                    Bind(AF_INET6, false, ignored);
                }
            }

            if (!opened)
            {
                Sockets.clear();

                if (error == WSAEADDRINUSE || error == WSAEACCES)
                {
                    State = OscListenState::PortInUse;
                    Owner = PortOwner(number);
                }
                else
                {
                    State = OscListenState::Failed;
                }

                return false;
            }

            State = everyNetwork ? OscListenState::EveryNetwork : OscListenState::ThisPcOnly;

            for (auto const& socket : Sockets)
            {
                auto const handle = socket.get();
                Threads.emplace_back([this, handle]() { ReceiveLoop(handle); });
            }

            return true;
        }
        catch (...)
        {
            Close();
            State = OscListenState::Failed;
            return false;
        }
    }

    void OscHub::Port::Close() noexcept
    {
        Closing = true;

        if (m_stop)
        {
            ::SetEvent(m_stop.get());
        }

        for (auto& thread : Threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }

        // Only once no thread can be using one, so a closed socket's handle is never reused under it.
        Threads.clear();
        Sockets.clear();
    }

    _Use_decl_annotations_
    void OscHub::Port::ReceiveLoop(SOCKET handle) noexcept
    {
        ::SetThreadDescription(::GetCurrentThread(), L"MIDI Patchbay OSC receiver");

        try
        {
            wil::unique_event_nothrow readable{};
            readable.reset(::CreateEventW(nullptr, FALSE, FALSE, nullptr));

            if (!readable || ::WSAEventSelect(handle, readable.get(), FD_READ) == SOCKET_ERROR)
            {
                return;
            }

            auto const buffer = std::make_unique<uint8_t[]>(osc::MaximumPacketBytes + 1);

            while (!Closing)
            {
                HANDLE const handles[2]{ m_stop.get(), readable.get() };

                if (::WaitForMultipleObjects(2, handles, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
                {
                    break;
                }

                // Everything that's waiting, until there's nothing left.
                while (!Closing)
                {
                    sockaddr_storage from{};
                    int fromLength{ sizeof(from) };

                    auto const received = ::recvfrom(handle, reinterpret_cast<char*>(buffer.get()),
                        static_cast<int>(osc::MaximumPacketBytes + 1), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);

                    if (received == SOCKET_ERROR)
                    {
                        auto const error = ::WSAGetLastError();

                        if (error == WSAEWOULDBLOCK)
                        {
                            break;
                        }

                        if (error == WSAEMSGSIZE || error == WSAECONNRESET)
                        {
                            continue;
                        }

                        // Anything else and the socket is no use.
                        return;
                    }

                    // Read the clock first, as close to arrival as there is.
                    auto clock = ReadClock();
                    auto const arrival = clock.Ticks;

                    auto const listeners = Listeners.load();

                    if (listeners == nullptr || received <= 0 || static_cast<size_t>(received) > osc::MaximumPacketBytes)
                    {
                        continue;
                    }

                    auto const sender = Normalized(from);

                    for (auto const& connection : *listeners)
                    {
                        connection->Receive(buffer.get(), static_cast<size_t>(received), sender, arrival, clock);
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    OscHub& OscHub::Current() noexcept
    {
        // Never destroyed. The routing engine is a static too, built before this, so it would
        // otherwise be destroyed after this and call it while shutting down.
        static OscHub* hub = new OscHub{};
        return *hub;
    }

    OscHub::~OscHub() noexcept
    {
        Shutdown();
    }

    bool OscHub::LetsEveryNetworkIn() const noexcept
    {
        using midiapp::firewall::FirewallStatus;

        if (!m_firewallRead)
        {
            return false;
        }

        // Unknown is a firewall that can't be read, which can't ask either.
        return m_firewall.Status == FirewallStatus::Open ||
            m_firewall.Status == FirewallStatus::Off ||
            m_firewall.Status == FirewallStatus::Unknown;
    }

    void OscHub::ApplyPortsLocked() noexcept
    {
        try
        {
            struct Wanted
            {
                bool EveryNetwork{ false };
                std::vector<winrt::com_ptr<Connection>> Listeners{};
            };

            std::map<uint16_t, Wanted> wanted{};

            for (auto const& [id, connection] : m_connections)
            {
                UNREFERENCED_PARAMETER(id);

                auto const settings = connection->Settings();

                if (settings.ReceivePort == 0)
                {
                    connection->SetListening(OscListenState::NotReceiving, false, 0, {});
                    continue;
                }

                auto& port = wanted[settings.ReceivePort];
                port.EveryNetwork = port.EveryNetwork || settings.ListenOn == OscListenOn::EveryNetwork;
                port.Listeners.push_back(connection);
            }

            auto const allowed = LetsEveryNetworkIn();

            for (auto it = m_ports.begin(); it != m_ports.end();)
            {
                if (wanted.count(it->first) == 0)
                {
                    it->second->Close();
                    it = m_ports.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            for (auto& [number, want] : wanted)
            {
                auto const everyNetwork = want.EveryNetwork && allowed;
                auto& port = m_ports[number];

                auto const usable = port != nullptr &&
                    (port->State == OscListenState::EveryNetwork || port->State == OscListenState::ThisPcOnly);

                // Opened again when it has to change, and when it couldn't open before.
                if (!usable || port->EveryNetwork != everyNetwork)
                {
                    if (port != nullptr)
                    {
                        port->Close();
                    }

                    port = std::make_unique<Port>();
                    port->Open(number, everyNetwork);
                }

                port->Listeners.store(std::make_shared<std::vector<winrt::com_ptr<Connection>> const>(want.Listeners));

                for (auto const& connection : want.Listeners)
                {
                    auto const settings = connection->Settings();
                    auto const held = settings.ListenOn == OscListenOn::EveryNetwork && !allowed &&
                        port->State == OscListenState::ThisPcOnly;

                    connection->SetListening(port->State, held, number, port->Owner);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to open the OSC ports.")
    }

    _Use_decl_annotations_
    void OscHub::Configure(std::vector<RouteOscConnection> const& connections) noexcept
    {
        // Stopping one waits for its worker, which can be in the middle of looking up a name, so
        // they're stopped after the lock is let go. The UI reads status under the same lock.
        std::vector<winrt::com_ptr<Connection>> stopping{};

        try
        {
            auto const wantsEveryNetwork = std::any_of(connections.begin(), connections.end(), [](RouteOscConnection const& connection)
                {
                    return connection.Settings.ReceivePort != 0 && connection.Settings.ListenOn == OscListenOn::EveryNetwork;
                });

            // Read before the first socket opens on every network, so Windows never has to ask.
            // Walks every rule, so not under the lock.
            std::optional<midiapp::firewall::FirewallState> firewall{};

            if (wantsEveryNetwork)
            {
                bool needed{ false };

                {
                    std::scoped_lock guard{ m_lock };
                    needed = !m_firewallRead;
                }

                if (needed)
                {
                    firewall = midiapp::firewall::QueryState(FirewallTarget());
                }
            }

            std::scoped_lock guard{ m_lock };

            if (!m_networkStarted && !connections.empty())
            {
                WSADATA data{};
                m_networkStarted = ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }

            m_configured = connections;

            if (firewall.has_value() && !m_firewallRead)
            {
                m_firewall = std::move(*firewall);
                m_firewallRead = true;
            }

            std::set<std::wstring> kept{};

            for (auto const& planned : connections)
            {
                kept.insert(planned.DeviceId);

                auto const found = m_connections.find(planned.DeviceId);

                if (found != m_connections.end())
                {
                    if (OscConnectionSignature(found->second->Settings()) != OscConnectionSignature(planned.Settings))
                    {
                        found->second->Update(planned);
                    }

                    continue;
                }

                auto connection = winrt::make_self<Connection>(planned);

                if (!connection->Start())
                {
                    MIDI_PATCHBAY_LOG_WARNING(L"Unable to start an OSC connection.");
                    continue;
                }

                m_connections.emplace(planned.DeviceId, std::move(connection));
            }

            for (auto it = m_connections.begin(); it != m_connections.end();)
            {
                if (kept.count(it->first) == 0)
                {
                    stopping.push_back(std::move(it->second));
                    it = m_connections.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            ApplyPortsLocked();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up the OSC connections.")

        for (auto const& connection : stopping)
        {
            connection->Stop();
        }
    }

    _Use_decl_annotations_
    winrt::com_ptr<IMidiEndpointConnectionRaw> OscHub::Endpoint(std::wstring const& deviceId) noexcept
    {
        std::scoped_lock guard{ m_lock };

        auto const found = m_connections.find(deviceId);

        if (found == m_connections.end())
        {
            return nullptr;
        }

        winrt::com_ptr<IMidiEndpointConnectionRaw> raw{};
        raw.copy_from(static_cast<IMidiEndpointConnectionRaw*>(found->second.get()));

        return raw;
    }

    void OscHub::Shutdown() noexcept
    {
        std::map<std::wstring, winrt::com_ptr<Connection>> stopping{};

        try
        {
            std::scoped_lock guard{ m_lock };

            for (auto& [number, port] : m_ports)
            {
                UNREFERENCED_PARAMETER(number);
                port->Close();
            }

            m_ports.clear();

            stopping.swap(m_connections);
            m_configured.clear();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close the OSC connections.")

        // Not under the lock: see Configure.
        for (auto const& [id, connection] : stopping)
        {
            UNREFERENCED_PARAMETER(id);
            connection->Stop();
        }
    }

    _Use_decl_annotations_
    std::optional<OscConnectionStatus> OscHub::Status(std::wstring const& deviceId) const noexcept
    {
        try
        {
            winrt::com_ptr<Connection> connection{};

            {
                std::scoped_lock guard{ m_lock };

                auto const found = m_connections.find(deviceId);

                if (found == m_connections.end())
                {
                    return std::nullopt;
                }

                connection = found->second;
            }

            return connection->Status();
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    void OscHub::ClearRecentlyHeard(std::wstring const& deviceId) noexcept
    {
        std::scoped_lock guard{ m_lock };

        if (auto const found = m_connections.find(deviceId); found != m_connections.end())
        {
            found->second->ClearRecentlyHeard();
        }
    }

    midiapp::firewall::FirewallTarget OscHub::FirewallTarget()
    {
        midiapp::firewall::FirewallTarget target{};

        target.ProgramPath = midiapp::firewall::CurrentProgramPath();
        target.ProgramFileName = L"\\" + std::filesystem::path{ target.ProgramPath }.filename().wstring();
        target.RuleName = std::wstring{ resources::GetString(L"OscFirewallRuleName") };
        target.RuleDescription = std::wstring{ resources::GetString(L"OscFirewallRuleDescription") };
        target.AllowSwitch = L"--allow-osc-through-firewall";

        return target;
    }

    void OscHub::RecheckFirewall() noexcept
    {
        try
        {
            // Walks every rule, so not under the lock.
            auto state = midiapp::firewall::QueryState(FirewallTarget());

            std::scoped_lock guard{ m_lock };

            auto const before = LetsEveryNetworkIn();

            m_firewall = std::move(state);
            m_firewallRead = true;

            if (before != LetsEveryNetworkIn())
            {
                ApplyPortsLocked();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read Windows Firewall.")
    }

    midiapp::firewall::FirewallState OscHub::Firewall() const noexcept
    {
        try
        {
            std::scoped_lock guard{ m_lock };
            return m_firewall;
        }
        catch (...)
        {
            return {};
        }
    }

    bool OscHub::IsHeldByFirewall() const noexcept
    {
        std::scoped_lock guard{ m_lock };

        if (LetsEveryNetworkIn())
        {
            return false;
        }

        return std::any_of(m_configured.begin(), m_configured.end(), [](RouteOscConnection const& connection)
            {
                return connection.Settings.ReceivePort != 0 && connection.Settings.ListenOn == OscListenOn::EveryNetwork;
            });
    }

    _Use_decl_annotations_
    bool OscHub::SendTestNote(OscConnectionSettings const& settings, uint8_t group) noexcept
    {
        try
        {
            if (settings.SendHost.empty() || settings.SendPort == 0 || OscRowOnGroup(settings, group) == nullptr)
            {
                return false;
            }

            WSADATA data{};

            if (::WSAStartup(MAKEWORD(2, 2), &data) != 0)
            {
                return false;
            }

            auto cleanup = wil::scope_exit([]() noexcept { ::WSACleanup(); });

            auto const addresses = Resolve(settings.SendHost, settings.SendPort);

            if (addresses.empty())
            {
                return false;
            }

            auto const handle = ConnectedSocket(addresses.front());

            if (!handle)
            {
                return false;
            }

            osc::OutboundTranslator translator{};
            uint64_t packets{ 0 };

            auto const send = [&](uint32_t word)
                {
                    std::vector<osc::MessageBuilder> out{};
                    translator.Translate(settings.Rows, &word, 1, out);
                    return !out.empty() && SendPackets(handle.get(), out, false, osc::Immediately, packets);
                };

            auto const groupBits = static_cast<uint32_t>(group & 0x0F) << 24;

            auto const on = send(0x20903C64u | groupBits);
            ::Sleep(TestNoteMilliseconds);
            auto const off = send(0x20803C00u | groupBits);

            return on && off;
        }
        catch (...)
        {
            return false;
        }
    }
}
