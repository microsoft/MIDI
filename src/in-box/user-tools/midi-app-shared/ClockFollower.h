// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Where an incoming MIDI clock has got to, so something can move in step with it between its
// pulses. Deliberately free of pch.h and WinRT, so the unit tests compile it unchanged. Times are
// in whatever units the caller counts in, as long as they only go forward.
//
// Pulses can arrive before the time they play: a clock generator in the same app hands them over
// early, with the timestamp they are meant for. So this keeps a short timeline of pulses rather
// than just the last one, and answers for any time on it.

#include <sal.h>
#include <array>
#include <cstdint>
#include <optional>

namespace midiapp
{
    // Timing clocks in a quarter note, and in the sixteenth note a song position counts in.
    constexpr uint32_t MidiClocksPerBeat = 24;
    constexpr uint32_t MidiClocksPerSongPositionStep = 6;

    class ClockFollower
    {
    public:
        // A timing clock that plays at this time.
        void Pulse(_In_ uint64_t time) noexcept;

        // Start at this time: the first pulse after it is the top of the song.
        void Start(_In_ uint64_t time) noexcept;

        // Only for a follower that keeps to Start and Stop: pulses from this time on don't count
        // until Continue or Start, so it holds where it got to.
        void Stop(_In_ uint64_t time) noexcept;

        // Picks up again from where Stop left it.
        void Continue() noexcept;

        // Waits for Start or Continue before it counts a pulse, and Stop holds it again.
        // Without this a follower counts every pulse and ignores Stop and Continue.
        void KeepsToStartAndStop(_In_ bool value) noexcept;

        bool IsStopped() const noexcept { return m_keepsToStartAndStop && m_stopped; }

        // Song position at this time, in sixteenth notes: the first pulse after it is that far in.
        void SongPosition(_In_ uint64_t time, _In_ uint32_t sixteenths) noexcept;

        // How far the clock has got at this time, in pulses from the top of the song. Between two
        // pulses it moves on at their pace, and past the last one at the pace of the last few,
        // but never as far as a pulse that hasn't arrived. Nothing before the first pulse.
        std::optional<double> PulsesAt(_In_ uint64_t time) const noexcept;

        // The latest time PulsesAt can say anything new about: one pulse past the last one.
        // Zero before the first pulse.
        uint64_t KnownUntil() const noexcept;

        // The pace of the last few pulses. Zero until two have arrived.
        uint64_t PulseInterval() const noexcept { return m_interval; }

    private:
        struct Mark
        {
            uint64_t Time{ 0 };
            uint64_t Number{ 0 };
        };

        // Half a second of pulses at 300 BPM is 60, and that is how far ahead the clock
        // generators schedule. This keeps a few beats more.
        static constexpr size_t Capacity = 256;

        Mark const& At(_In_ size_t index) const noexcept;
        void Restart(_In_ uint64_t time, _In_ uint64_t nextNumber) noexcept;
        void Measure(_In_ uint64_t gap) noexcept;

        // Oldest first.
        std::array<Mark, Capacity> m_marks{};
        size_t m_first{ 0 };
        size_t m_count{ 0 };

        uint64_t m_nextNumber{ 0 };

        uint64_t m_interval{ 0 };
        uint64_t m_lastGap{ 0 };

        bool m_keepsToStartAndStop{ false };
        bool m_stopped{ true };
    };
}
