// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Which local address each remote reached a socket on. A socket bound to every address leaves
// the source of a reply to Windows, and a remote which only accepts its peer's address drops a
// reply from any other. RFC 1122 4.1.3.5 asks for the address the request was sent to.
//
// No WIL and no tracing, so the tests can include it on its own.
// ============================================================================

#pragma once

#include <array>
#include <atomic>
#include <compare>
#include <map>
#include <shared_mutex>

namespace RtpMidiNet
{
    // Where a datagram arrived: the local address and the interface it came in on
    struct LocalAddress
    {
        int Family{ 0 };
        IN_ADDR IPv4{};
        IN6_ADDR IPv6{};
        ULONG InterfaceIndex{ 0 };

        bool operator==(_In_ LocalAddress const& other) const noexcept
        {
            return Family == other.Family && InterfaceIndex == other.InterfaceIndex &&
                memcmp(&IPv4, &other.IPv4, sizeof(IPv4)) == 0 && memcmp(&IPv6, &other.IPv6, sizeof(IPv6)) == 0;
        }
    };

    // Anyone can send to a host, so the table is bounded. When it is full, the remote heard from
    // least recently makes room, so strangers cannot push out a remote which is still talking.
    class ReplySourceTable
    {
    public:
        explicit ReplySourceTable(_In_ size_t const capacity) noexcept : m_capacity(capacity) {}

        ReplySourceTable(_In_ ReplySourceTable const&) = delete;
        ReplySourceTable& operator=(_In_ ReplySourceTable const&) = delete;

        // now is any clock which only goes forward, such as GetTickCount64
        void Remember(
            _In_ RtpMidi::PeerAddress const& from,
            _In_ LocalAddress const& local,
            _In_ uint64_t const now) noexcept
        {
            try
            {
                auto const key = KeyFor(from);

                {
                    auto lock = std::shared_lock{ m_lock };

                    auto const it = m_entries.find(key);

                    if (it != m_entries.end() && it->second.Local == local)
                    {
                        it->second.LastHeard.store(now, std::memory_order_relaxed);
                        return;
                    }
                }

                auto lock = std::unique_lock{ m_lock };

                auto it = m_entries.find(key);

                if (it == m_entries.end())
                {
                    if (m_capacity == 0) return;

                    if (m_entries.size() >= m_capacity) EvictLeastRecentlyHeard();

                    it = m_entries.try_emplace(key).first;
                }

                // a remote can reach this PC at another of its addresses
                it->second.Local = local;
                it->second.LastHeard.store(now, std::memory_order_relaxed);
            }
            catch (...)
            {
                // only costs the reply address
            }
        }

        bool TryGet(_In_ RtpMidi::PeerAddress const& to, _Out_ LocalAddress& local) noexcept
        {
            local = LocalAddress{};

            try
            {
                auto lock = std::shared_lock{ m_lock };

                auto const it = m_entries.find(KeyFor(to));
                if (it == m_entries.end()) return false;

                local = it->second.Local;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        size_t Count() noexcept
        {
            try
            {
                auto lock = std::shared_lock{ m_lock };

                return m_entries.size();
            }
            catch (...)
            {
                return 0;
            }
        }

    private:
        struct PeerKey
        {
            uint8_t Family{ 0 };
            std::array<uint8_t, 16> Bytes{};
            uint16_t Port{ 0 };
            uint32_t ScopeId{ 0 };

            auto operator<=>(_In_ PeerKey const&) const = default;
        };

        struct Entry
        {
            LocalAddress Local{};
            std::atomic<uint64_t> LastHeard{ 0 };
        };

        static PeerKey KeyFor(_In_ RtpMidi::PeerAddress const& address) noexcept
        {
            PeerKey key{};
            key.Family = static_cast<uint8_t>(address.Family);
            memcpy(key.Bytes.data(), address.Bytes.data(), (std::min)(key.Bytes.size(), address.Bytes.size()));
            key.Port = address.Port;
            key.ScopeId = address.ScopeId;

            return key;
        }

        // The caller holds the lock exclusively
        void EvictLeastRecentlyHeard() noexcept
        {
            auto oldest = m_entries.end();

            for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
            {
                if (oldest == m_entries.end() ||
                    it->second.LastHeard.load(std::memory_order_relaxed) < oldest->second.LastHeard.load(std::memory_order_relaxed))
                {
                    oldest = it;
                }
            }

            if (oldest != m_entries.end()) m_entries.erase(oldest);
        }

        size_t const m_capacity;

        std::shared_mutex m_lock;
        std::map<PeerKey, Entry> m_entries;
    };
}
