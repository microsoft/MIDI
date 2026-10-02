// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// WORKAROUND for the Windows DNS client's DNS-SD announcements, shared by the Network MIDI 2.0
// and rtpMIDI transports. Remove it once the DNS client is fixed.
//
// When a registration completes, the DNS client announces it once, where RFC 6762 section 8.3
// asks for at least two announcements a second apart, and it sets the cache-flush bit on the
// shared PTR record, which section 10.2 forbids. A device that misses that one packet, which is
// easy on Wi-Fi, doesn't list the new host until it asks again, and a Mac can go a long time
// without asking. The flush bit also makes every device that hears it drop the other instances
// of the service type, including hosts this PC registered earlier. Answers to queries are
// correct, so only the announcements need help.
//
// So after each registration, the PTR record of every host still registered is announced again,
// twice, without the flush bit. Only the PTR records: the SRV, TXT and address records are
// unique, the DNS client owns them and answers for them correctly, and a copy that differed from
// its own in any byte would flush the real one.
//
// A host limited to one network adapter is only announced on that adapter.
// ============================================================================

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace WindowsMidiServicesInternal
{
    // the TTL the DNS client gives its own PTR records
    constexpr uint32_t MidiDnssdAnnouncedPtrTtlSeconds = 4500;

    // inside the smallest link an IPv6 network may have
    constexpr size_t MidiDnssdAnnouncementMaxPacketBytes = 1200;

    // The first repeat waits out the second in which the flush bit takes effect (RFC 6762
    // section 10.2), so what it restores is not dropped along with everything else.
    constexpr uint64_t MidiDnssdFirstRepeatDelayMilliseconds = 1500;
    constexpr uint64_t MidiDnssdSecondRepeatDelayMilliseconds = 4500;


    inline std::string MidiDnssdToUtf8(_In_ std::wstring const& text)
    {
        if (text.empty()) return {};

        auto const size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};

        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);

        return result;
    }


    // mDNS responses carrying one PTR record per label, such as
    // "_midi2._udp.local PTR Studio PC._midi2._udp.local", split so that none is larger than
    // maxPacketBytes. serviceType and the labels are UTF-8. A label that is empty, longer than 63
    // bytes or holds a period is left out.
    inline std::vector<std::vector<uint8_t>> BuildDnssdPtrAnnouncements(
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


    struct MidiDnssdAnnouncementResult
    {
        // interfaces that every packet went out on
        uint32_t IPv4Interfaces{ 0 };
        uint32_t IPv6Interfaces{ 0 };

        // the last Winsock or IP Helper error, or 0
        int LastError{ 0 };
    };

    inline uint32_t SendDnssdAnnouncementsOnInterfaces(
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

    // Multicasts the packets on every interface that is up and carries multicast, over IPv4 and
    // IPv6, or only on the one adapter when adapterId is not an empty GUID. Winsock must already
    // be started.
    inline MidiDnssdAnnouncementResult SendDnssdAnnouncements(
        _In_ std::vector<std::vector<uint8_t>> const& packets,
        _In_ GUID const& adapterId = GUID{})
    {
        MidiDnssdAnnouncementResult result{};
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

            if (!IsEqualGUID(adapterId, GUID{}))
            {
                GUID id{};
                if (ConvertInterfaceLuidToGuid(&adapter->Luid, &id) != NO_ERROR || !IsEqualGUID(id, adapterId)) continue;
            }

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

        result.IPv4Interfaces = SendDnssdAnnouncementsOnInterfaces(AF_INET, ipv4Interfaces, packets, result.LastError);
        result.IPv6Interfaces = SendDnssdAnnouncementsOnInterfaces(AF_INET6, ipv6Interfaces, packets, result.LastError);

        return result;
    }


    // Repeats the announcement of every registration this process holds, a little after each new
    // one. Registrations may be added and withdrawn on any thread.
    class MidiDnssdFollowUpAnnouncer
    {
    public:
        // Called once for each adapter that has hosts limited to it, and once with GUID_NULL for
        // the hosts on every adapter
        using Sender = std::function<MidiDnssdAnnouncementResult(std::vector<std::vector<uint8_t>> const&, GUID const& adapterId)>;
        using SentHandler = std::function<void(size_t hostCount, size_t packetCount, MidiDnssdAnnouncementResult const& result)>;

        MidiDnssdFollowUpAnnouncer() = default;
        ~MidiDnssdFollowUpAnnouncer() { Stop(); }

        MidiDnssdFollowUpAnnouncer(_In_ MidiDnssdFollowUpAnnouncer const&) = delete;
        MidiDnssdFollowUpAnnouncer& operator=(_In_ MidiDnssdFollowUpAnnouncer const&) = delete;

        // serviceType is the full query name, such as "_midi2._udp.local". onSent runs on the
        // announcer's own thread after each repeat. A sender replaces the network, for tests.
        HRESULT Start(
            _In_ std::wstring const& serviceType,
            _In_ SentHandler onSent = nullptr,
            _In_ Sender sender = nullptr,
            _In_ uint64_t const firstDelayMilliseconds = MidiDnssdFirstRepeatDelayMilliseconds,
            _In_ uint64_t const secondDelayMilliseconds = MidiDnssdSecondRepeatDelayMilliseconds) noexcept
        {
            try
            {
                auto lock = std::scoped_lock{ m_lock };

                if (m_running) return S_FALSE;

                WSADATA wsaData{};
                auto const wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
                if (wsaResult != 0) return HRESULT_FROM_WIN32(wsaResult);

                m_winsockStarted = true;

                m_serviceType = MidiDnssdToUtf8(serviceType);
                m_onSent = std::move(onSent);
                m_sender = std::move(sender);
                m_firstDelayMilliseconds = firstDelayMilliseconds;
                m_secondDelayMilliseconds = secondDelayMilliseconds;
                m_labels.clear();
                m_firstRepeatDue = 0;
                m_lastRepeatDue = 0;

                m_thread = std::jthread([this](std::stop_token stopToken) { Worker(stopToken); });
                m_running = true;

                return S_OK;
            }
            catch (...)
            {
                return E_FAIL;
            }
        }

        // Nothing is sent once this returns
        void Stop() noexcept
        {
            std::jthread thread;
            bool winsockStarted{ false };

            {
                auto lock = std::scoped_lock{ m_lock };

                m_running = false;
                m_labels.clear();
                m_firstRepeatDue = 0;
                m_lastRepeatDue = 0;

                thread = std::move(m_thread);
                winsockStarted = std::exchange(m_winsockStarted, false);
            }

            thread.request_stop();

            if (thread.joinable())
            {
                if (thread.get_id() != std::this_thread::get_id()) thread.join();
                else thread.detach();
            }

            if (winsockStarted) WSACleanup();
        }

        // After a registration completes, with the label the DNS client actually registered. A
        // host limited to one adapter passes that adapter, and is only announced there.
        void AddRegistration(_In_ std::wstring_view const instanceLabel, _In_ GUID const& adapterId = GUID{})
        {
            if (instanceLabel.empty()) return;

            {
                auto lock = std::scoped_lock{ m_lock };

                if (!m_running) return;

                auto const existing = FindLabel(instanceLabel);

                if (existing == m_labels.end()) m_labels.push_back(Registration{ std::wstring{ instanceLabel }, adapterId });
                else existing->AdapterId = adapterId;

                auto const now = GetTickCount64();

                // the first repeat stays put during a burst of registrations, and the last one
                // always comes after the burst's final flush
                if (m_firstRepeatDue == 0) m_firstRepeatDue = now + m_firstDelayMilliseconds;
                m_lastRepeatDue = now + m_secondDelayMilliseconds;

                m_generation++;
            }

            m_changed.notify_all();
        }

        // Before a registration is withdrawn. Waits out a repeat in progress, so none can follow
        // the goodbye and bring the host back into other devices' lists.
        void RemoveRegistration(_In_ std::wstring_view const instanceLabel) noexcept
        {
            auto lock = std::scoped_lock{ m_lock };

            auto const it = FindLabel(instanceLabel);
            if (it != m_labels.end()) m_labels.erase(it);
        }

    private:
        struct Registration
        {
            std::wstring Label;
            GUID AdapterId{};
        };

        std::vector<Registration>::iterator FindLabel(_In_ std::wstring_view const instanceLabel) noexcept
        {
            // DNS names compare without regard to case
            return std::find_if(m_labels.begin(), m_labels.end(), [&](Registration const& registration)
            {
                return CompareStringOrdinal(registration.Label.data(), static_cast<int>(registration.Label.size()),
                    instanceLabel.data(), static_cast<int>(instanceLabel.size()), TRUE) == CSTR_EQUAL;
            });
        }

        void Worker(_In_ std::stop_token stopToken) noexcept
        {
            try
            {
                auto lock = std::unique_lock{ m_lock };

                while (!stopToken.stop_requested())
                {
                    auto const now = GetTickCount64();

                    uint64_t next{ m_firstRepeatDue };
                    if (m_lastRepeatDue != 0 && (next == 0 || m_lastRepeatDue < next)) next = m_lastRepeatDue;

                    if (next == 0 || next > now)
                    {
                        auto const generation = m_generation;
                        auto const changed = [&]() { return m_generation != generation; };

                        if (next == 0) m_changed.wait(lock, stopToken, changed);
                        else m_changed.wait_for(lock, stopToken, std::chrono::milliseconds(static_cast<int64_t>(next - now)), changed);

                        continue;
                    }

                    if (m_firstRepeatDue != 0 && m_firstRepeatDue <= now) m_firstRepeatDue = 0;
                    if (m_lastRepeatDue != 0 && m_lastRepeatDue <= now) m_lastRepeatDue = 0;

                    if (m_labels.empty()) continue;

                    size_t const hostCount{ m_labels.size() };
                    size_t packetCount{ 0 };
                    MidiDnssdAnnouncementResult result{};

                    try
                    {
                        // the hosts on every adapter first, then each adapter's own, in the order they came
                        std::vector<GUID> adapters{ GUID{} };

                        for (auto const& registration : m_labels)
                        {
                            if (std::none_of(adapters.begin(), adapters.end(), [&](GUID const& id) { return IsEqualGUID(id, registration.AdapterId) != FALSE; }))
                            {
                                adapters.push_back(registration.AdapterId);
                            }
                        }

                        for (auto const& adapterId : adapters)
                        {
                            std::vector<std::string> labels;

                            for (auto const& registration : m_labels)
                            {
                                if (IsEqualGUID(registration.AdapterId, adapterId)) labels.push_back(MidiDnssdToUtf8(registration.Label));
                            }

                            if (labels.empty()) continue;

                            auto const packets = BuildDnssdPtrAnnouncements(
                                m_serviceType, labels, MidiDnssdAnnouncedPtrTtlSeconds, MidiDnssdAnnouncementMaxPacketBytes);

                            packetCount += packets.size();

                            // sent with the lock held, which is what makes RemoveRegistration wait for it
                            auto const sent = m_sender ? m_sender(packets, adapterId) : SendDnssdAnnouncements(packets, adapterId);

                            result.IPv4Interfaces += sent.IPv4Interfaces;
                            result.IPv6Interfaces += sent.IPv6Interfaces;
                            if (sent.LastError != 0) result.LastError = sent.LastError;
                        }
                    }
                    catch (...)
                    {
                        result.LastError = ERROR_NOT_ENOUGH_MEMORY;
                    }

                    auto const onSent = m_onSent;

                    lock.unlock();

                    if (onSent) onSent(hostCount, packetCount, result);

                    lock.lock();
                }
            }
            catch (...)
            {
                // a failed lock or wait: the repeats stop, and registration itself is unaffected
            }
        }

        std::mutex m_lock;
        std::condition_variable_any m_changed;

        bool m_running{ false };
        bool m_winsockStarted{ false };
        std::string m_serviceType;
        SentHandler m_onSent;
        Sender m_sender;
        uint64_t m_firstDelayMilliseconds{ MidiDnssdFirstRepeatDelayMilliseconds };
        uint64_t m_secondDelayMilliseconds{ MidiDnssdSecondRepeatDelayMilliseconds };

        std::vector<Registration> m_labels;
        uint64_t m_firstRepeatDue{ 0 };
        uint64_t m_lastRepeatDue{ 0 };
        uint64_t m_generation{ 0 };

        std::jthread m_thread;
    };
}
