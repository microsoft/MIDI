// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TapTempo.h"
#include "ClockStore.h"

namespace midiclock
{
    void TapTempo::Reset() noexcept
    {
        try
        {
            m_taps.clear();
        }
        catch (...)
        {
        }
    }

    std::optional<double> TapTempo::Tap() noexcept
    {
        try
        {
            auto const now = std::chrono::steady_clock::now();

            if (!m_taps.empty() && (now - m_taps.back()) > RestartAfter)
            {
                m_taps.clear();
            }

            m_taps.push_back(now);

            if (m_taps.size() > MaximumTapsConsidered)
            {
                m_taps.erase(m_taps.begin(), m_taps.end() - MaximumTapsConsidered);
            }

            if (m_taps.size() < 2)
            {
                return std::nullopt;
            }

            auto const span = std::chrono::duration_cast<std::chrono::duration<double>>(
                m_taps.back() - m_taps.front()).count();

            auto const intervals = static_cast<double>(m_taps.size() - 1);

            if (span <= 0.0 || intervals <= 0.0)
            {
                return std::nullopt;
            }

            auto const beatsPerMinute = 60.0 * intervals / span;

            return std::clamp(beatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
}
