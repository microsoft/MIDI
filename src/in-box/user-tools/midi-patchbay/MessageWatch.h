// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What a connection monitor window needs from routing: a copy of each message that goes along
// the links it watches. Routing runs on the service callback thread with no locks and no
// allocation, so the copy goes into a fixed-size buffer that never makes routing wait. When the
// window falls behind and the buffer is full, the message is dropped and counted instead.
//
// Pure, so the tests can prove it, with no XAML and no precompiled header.

#include <sal.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

namespace midipatchbay
{
    // How many messages wait for a window between two looks. The window looks about 20 times a
    // second, so this is plenty for anything a person can read.
    constexpr uint32_t WatchBufferCapacity = 4096;

    // Windows watching one link at once. More than this on one link are left out.
    constexpr size_t MaximumWatchersPerLink = 8;

    // What a window keeps. Older messages go as new ones come.
    constexpr size_t MaximumWatchHistory = 5000;

    struct WatchedMessage
    {
        uint64_t Timestamp{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t WordCount{ 0 };

        // Which of the window's links it went along.
        uint16_t Link{ 0 };

        // Made by Test, not sent to a device.
        bool FromTest{ false };
    };

    // Any number of routing threads put messages in, and one window thread takes them out.
    // Putting in never waits and never allocates: a full buffer drops the message and counts it.
    //
    // A buffer is handed from one window to the next rather than freed, because a routing thread
    // can still hold it for a moment after a window lets go. Each window gets its own lease, and
    // what was put in under an earlier lease is never handed to a later window.
    class WatchBuffer
    {
    public:
        WatchBuffer();

        WatchBuffer(WatchBuffer const&) = delete;
        WatchBuffer& operator=(WatchBuffer const&) = delete;

        // Hands the buffer to a new window, and returns its lease. The drop count starts over.
        uint32_t Lease() noexcept;

        uint32_t CurrentLease() const noexcept;

        // Any thread. False when the buffer is full, and the message was dropped.
        bool Push(
            _In_ uint32_t lease,
            _In_ uint16_t link,
            _In_ uint64_t timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept;

        // One thread only. Takes up to capacity messages, oldest first, and returns how many.
        // Messages from an earlier lease are thrown away on the way.
        size_t Drain(_Out_writes_to_(capacity, return) WatchedMessage* messages, _In_ size_t capacity) noexcept;

        // Dropped under the current lease.
        uint64_t Dropped() const noexcept;

    private:
        struct Slot
        {
            std::atomic<uint64_t> Sequence{ 0 };
            uint32_t Lease{ 0 };
            WatchedMessage Message{};
        };

        std::unique_ptr<Slot[]> m_slots{};

        std::atomic<uint64_t> m_putPosition{ 0 };
        uint64_t m_takePosition{ 0 };

        std::atomic<uint32_t> m_lease{ 0 };
        std::atomic<uint64_t> m_dropped{ 0 };
    };

    // One window watching one link.
    struct WatchTarget
    {
        WatchBuffer* Buffer{ nullptr };
        uint32_t Lease{ 0 };
        uint16_t Link{ 0 };
    };

    // Every window watching one link. Never changed once routing can see it: a change makes a
    // new one.
    struct WatchFan
    {
        std::array<WatchTarget, MaximumWatchersPerLink> Targets{};
        size_t Count{ 0 };

        // False when the link already has as many watchers as it can.
        bool Add(_In_ WatchTarget const& target) noexcept;

        // On the routing thread.
        void Deliver(
            _In_ uint64_t timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) const noexcept;
    };

    // What a window hands out to the engine to start watching, and gets back.
    struct WatchLease
    {
        uint32_t Id{ 0 };
        WatchBuffer* Buffer{ nullptr };
        uint32_t Lease{ 0 };
    };

    enum class WatchedKind : int32_t
    {
        Other = 0,

        // Timing clock, and the jitter reduction clock and timestamps, as the Monitor counts it.
        Clock = 1,

        ActiveSensing = 2,
    };

    WatchedKind ClassifyWatched(_In_ uint32_t word0) noexcept;

    // "20903C64 00000000", the way the Monitor shows the words.
    std::wstring FormatWatchedWords(
        _In_ std::array<uint32_t, 4> const& words,
        _In_ uint8_t wordCount);

    // A timestamp in milliseconds after the origin, or before it when it is earlier. A timestamp
    // of 0 means "now", which the caller decides.
    double WatchMilliseconds(
        _In_ uint64_t timestamp,
        _In_ uint64_t origin,
        _In_ uint64_t ticksPerSecond) noexcept;

    struct WatchRow
    {
        uint64_t Sequence{ 0 };
        WatchedMessage Message{};
    };

    // The last messages a window shows, each with a number that stays with it, so the list can
    // find a row however many have gone from the top since.
    class WatchHistory
    {
    public:
        explicit WatchHistory(_In_ size_t limit = MaximumWatchHistory) noexcept;

        // Returns how many of the oldest rows went to make room: 0 or 1.
        size_t Add(_In_ WatchedMessage const& message);

        WatchRow const* Find(_In_ uint64_t sequence) const noexcept;

        size_t Size() const noexcept { return m_rows.size(); }

        // The newest row's number. 0 before the first.
        uint64_t LastSequence() const noexcept { return m_nextSequence - 1; }

        void Clear() noexcept;

    private:
        std::deque<WatchRow> m_rows{};
        size_t m_limit{ MaximumWatchHistory };
        uint64_t m_nextSequence{ 1 };
    };
}
