// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// When each sample of a running LFO is due, and where in its cycle it falls. Deliberately free of
// pch.h, WinRT and XAML, so the unit tests compile it unchanged. MIDI Glass counts in microseconds
// and MIDI Patchbay in MIDI timestamp ticks; this does not care which, as long as it is told how
// many make a second.
//
// Every sample is placed against one origin rather than against the sample before it, so a late
// wakeup is caught up instead of accumulated. A tempo change starts a new origin at the next sample
// and carries the phase over, so a faster sweep does not jump back to the start of its cycle.

#include <sal.h>
#include <cstdint>

namespace midiapp
{
    class LfoSweep
    {
    public:
        // Starts the sweep at the top of its cycle, with the first sample due at origin.
        void Begin(
            _In_ uint64_t origin,
            _In_ uint64_t unitsPerSecond,
            _In_ double beatsPerCycle,
            _In_ double beatsPerMinute,
            _In_ int32_t intervalMilliseconds) noexcept;

        // Time between two samples, in the caller's units.
        uint64_t Interval() const noexcept;

        // When the next sample is due, and where in the cycle it falls, 0 to 1.
        uint64_t NextDue() const noexcept;
        double NextPhase() const noexcept;

        // The next sample has gone.
        void Advance() noexcept;

        uint64_t SamplesSent() const noexcept { return m_samplesSent; }
        double BeatsPerMinute() const noexcept { return m_beatsPerMinute; }

        // New rate or spacing for the samples still to come. The next sample keeps its time and
        // the phase the sweep has reached, so nothing jumps.
        void Retime(
            _In_ double beatsPerCycle,
            _In_ double beatsPerMinute,
            _In_ int32_t intervalMilliseconds) noexcept;

        // A wakeup more than eight samples late starts again from now, at the phase the sweep
        // had reached, rather than sending everything it missed in one burst. A page fault must
        // not turn into forty messages at once. Returns true when it did.
        bool CatchUp(_In_ uint64_t now) noexcept;

    private:
        double PhaseOf(_In_ uint64_t sampleIndex) const noexcept;

        uint64_t m_origin{ 0 };
        uint64_t m_unitsPerSecond{ 1'000'000 };
        uint64_t m_samplesSent{ 0 };

        // How far through the cycle the origin already was.
        double m_phaseAtOrigin{ 0.0 };

        double m_beatsPerCycle{ 4.0 };
        double m_beatsPerMinute{ 120.0 };
        int32_t m_intervalMilliseconds{ 25 };
    };
}
