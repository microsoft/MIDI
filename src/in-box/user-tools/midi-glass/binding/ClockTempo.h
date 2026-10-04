// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK, so the unit tests compile it unchanged.

#include <sal.h>
#include <array>
#include <cstdint>
#include <optional>

#include "LayoutModel.h"

namespace glass
{
    // A pause longer than this between two clock messages is a clock that stopped, not a slow one.
    constexpr uint64_t ClockGapMicroseconds = 1'000'000;

    // How far a measured tempo has to move before anything is told about it. Small enough to
    // follow a tempo ramp, big enough that the jitter on each message does not chatter.
    constexpr double ClockTempoHysteresis = 0.2;

    // Half a beat of clock has to arrive before the tempo it gives is believed.
    constexpr size_t MinimumClockTicksForTempo = ClockTicksPerQuarterNote / 2 + 1;

    // The tempo of a MIDI clock, worked out as its messages arrive.
    //
    // Measured over the last quarter note, which smooths out the jitter a cable and a busy PC
    // add to each message. A gap starts the measurement again, so a clock that stops and comes
    // back later is not read as one very long beat, and the last tempo is kept meanwhile.
    class ClockTempoMeter
    {
    public:
        // One clock message, at this time on any steady clock. A tempo when there is a new one
        // worth passing on, rounded to a tenth; nothing while it is settling or has not moved.
        std::optional<double> Tick(_In_ uint64_t microseconds) noexcept;

        // Forgets the messages, and the tempo too.
        void Reset() noexcept;

        // The last tempo passed on, or zero before there was one.
        double BeatsPerMinute() const noexcept { return m_published; }

    private:
        std::array<uint64_t, ClockTicksPerQuarterNote + 1> m_times{};
        size_t m_count{ 0 };
        size_t m_next{ 0 };
        double m_published{ 0.0 };
    };
}
