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
    class TapTempo
    {
    public:
        // Empty until there are at least two taps close enough together to mean anything.
        std::optional<double> Tap() noexcept;

        void Reset() noexcept;

        size_t TapCount() const noexcept { return m_taps.size(); }

    private:
        // Longer than a tap at the slowest tempo this app allows, so a pause is a fresh start
        // rather than one enormous interval.
        static constexpr std::chrono::milliseconds RestartAfter{ 3500 };

        static constexpr size_t MaximumTapsConsidered = 8;

        std::vector<std::chrono::steady_clock::time_point> m_taps{};
    };
}
