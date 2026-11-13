// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Tempo from the spacing of repeated taps, shared by the MIDI Clock app and the MIDI Calculator.
// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include <sal.h>

#include <chrono>
#include <optional>
#include <vector>

namespace midiapp
{
    // Only the recent taps count, so a customer can settle into the beat without the first few
    // attempts holding the answer back.
    //
    // A gap that does not belong to the tempo being tapped - a pause, a hesitation, a slip, or
    // the customer deciding on a different tempo - starts a fresh sequence rather than being
    // averaged in. The next tap after that establishes the new tempo.
    class TapTempo
    {
    public:
        // The answer is rounded to resolution (0.5 BPM in the MIDI Clock app) and clamped to
        // the range.
        TapTempo(
            _In_ double minimumBeatsPerMinute,
            _In_ double maximumBeatsPerMinute,
            _In_ double resolution) noexcept;

        // Empty until there are at least two taps close enough together to mean anything.
        std::optional<double> Tap() noexcept;

        // The same, for a tap that happened at a known time, such as a MIDI note's timestamp.
        std::optional<double> TapAt(_In_ std::chrono::steady_clock::time_point const& when) noexcept;

        void Reset() noexcept;

        size_t TapCount() const noexcept { return m_taps.size(); }

        // How far the taps strayed from an even beat: the largest difference, in milliseconds,
        // between any one interval and their average. Empty with fewer than three taps.
        std::optional<double> SpreadMilliseconds() const noexcept;

        static constexpr size_t MaximumTapsConsidered = 8;

    private:
        // True when this tap does not continue the tempo already being tapped.
        bool IsNewSequence(_In_ std::chrono::steady_clock::time_point const& now) const noexcept;

        // Backstop for when there is only one tap to go on and no tempo to compare against.
        // Longer than a tap at the slowest tempo the MIDI Clock app allows.
        static constexpr std::chrono::milliseconds RestartAfter{ 3500 };

        // How far an interval may sit either side of the tempo already being tapped before it
        // is treated as the start of a new one. Wide enough for ordinary human unevenness,
        // narrow enough to catch a pause or a deliberate change of tempo.
        static constexpr double OutlierLowFactor = 0.55;
        static constexpr double OutlierHighFactor = 1.75;

        double m_minimum;
        double m_maximum;
        double m_resolution;

        std::vector<std::chrono::steady_clock::time_point> m_taps{};
    };
}
