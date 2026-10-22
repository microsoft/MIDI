// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// OSC time tags and MIDI timestamps. A time tag is NTP time: seconds since January 1, 1900 in its
// high 32 bits and a fraction of a second in its low 32. A MIDI timestamp is a QueryPerformance-
// Counter tick. The two meet through the wall clock: a FILETIME read at the same moment as the
// tick count. Pure, for the unit tests.

#include <sal.h>

#include <cstdint>

namespace midipatchbay::osc
{
    // Seconds from January 1, 1601 (FILETIME) to January 1, 1900 (NTP).
    constexpr uint64_t SecondsFrom1601To1900 = 9'435'484'800ull;

    constexpr uint64_t FileTimeTicksPerSecond = 10'000'000ull;

    // The wall clock and the tick count, read together.
    struct ClockPair
    {
        // 100 ns units since January 1, 1601, UTC.
        uint64_t FileTime{ 0 };

        uint64_t Ticks{ 0 };
        uint64_t TicksPerSecond{ 0 };
    };

    // NTP's seconds count wraps on February 7, 2036. A count below 2^31 is read as after that,
    // which is right from 1968 until 2104.
    uint64_t FileTimeFromTimeTag(_In_ uint64_t timeTag) noexcept;
    uint64_t TimeTagFromFileTime(_In_ uint64_t fileTime) noexcept;

    // How far apart two times are is limited to a day either way, so arithmetic never overflows.
    // A time tag of 0 or Immediately is the clock pair's own time.
    uint64_t TicksFromTimeTag(_In_ uint64_t timeTag, _In_ ClockPair const& clock) noexcept;
    uint64_t TimeTagFromTicks(_In_ uint64_t ticks, _In_ ClockPair const& clock) noexcept;

    // Learns how far behind a sender's time tags arrive, so its messages can keep the spacing it
    // gave them without the two clocks agreeing. Each message plays at its tag, plus the smallest
    // gap seen between a tag and its arrival over the last few seconds, plus a fixed delay that
    // leaves room for network jitter. Tags are measured from the first one seen, not from the
    // wall clock, so a sender whose clock is days or years off still keeps its spacing.
    class SpacingKeeper
    {
    public:
        // The gap is forgotten after two windows.
        static constexpr uint64_t WindowMilliseconds = 2000;

        // When the message should play, in ticks. Never before arrival, and never more than the
        // delay after the earliest the sender's spacing allows.
        uint64_t PlayAt(
            _In_ uint64_t timeTag,
            _In_ uint64_t arrivalTicks,
            _In_ uint64_t delayTicks,
            _In_ uint64_t ticksPerSecond) noexcept;

        void Reset() noexcept;

    private:
        // The first tag seen and its arrival, which later tags are measured from.
        uint64_t m_anchorTag{ 0 };
        int64_t m_anchorTicks{ 0 };

        // arrival - tag, which can be negative when the sender's clock is ahead.
        int64_t m_currentSmallest{ 0 };
        int64_t m_previousSmallest{ 0 };
        bool m_hasCurrent{ false };
        bool m_hasPrevious{ false };
        uint64_t m_windowStart{ 0 };
    };
}
