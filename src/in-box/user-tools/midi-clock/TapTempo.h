// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <chrono>

namespace midiclock
{
    // Tempo from the spacing of repeated taps. Only the recent taps count, so a customer can
    // settle into the beat without the first few attempts holding the answer back.
    //
    // A gap that does not belong to the tempo being tapped - a pause, a hesitation, a slip, or
    // the customer deciding on a different tempo - starts a fresh sequence rather than being
    // averaged in. The next tap after that establishes the new tempo.
    class TapTempo
    {
    public:
        // Empty until there are at least two taps close enough together to mean anything.
        std::optional<double> Tap() noexcept;

        void Reset() noexcept;

        size_t TapCount() const noexcept { return m_taps.size(); }

    private:
        // True when this tap does not continue the tempo already being tapped.
        bool IsNewSequence(_In_ std::chrono::steady_clock::time_point const& now) const noexcept;
        // Backstop for when there is only one tap to go on and no tempo to compare against.
        // Longer than a tap at the slowest tempo this app allows.
        static constexpr std::chrono::milliseconds RestartAfter{ 3500 };

        static constexpr size_t MaximumTapsConsidered = 8;

        // How far an interval may sit either side of the tempo already being tapped before it
        // is treated as the start of a new one. Wide enough for ordinary human unevenness,
        // narrow enough to catch a pause or a deliberate change of tempo.
        static constexpr double OutlierLowFactor = 0.55;
        static constexpr double OutlierHighFactor = 1.75;

        // Nobody taps accurately enough to mean the digits beyond this, and it matches the step
        // the tempo slider moves in.
        static constexpr double Resolution = 0.5;

        std::vector<std::chrono::steady_clock::time_point> m_taps{};
    };
}
