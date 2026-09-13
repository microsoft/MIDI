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
    namespace
    {
        double SecondsBetween(
            _In_ std::chrono::steady_clock::time_point const& from,
            _In_ std::chrono::steady_clock::time_point const& to) noexcept
        {
            return std::chrono::duration_cast<std::chrono::duration<double>>(to - from).count();
        }
    }

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

    _Use_decl_annotations_
    bool TapTempo::IsNewSequence(std::chrono::steady_clock::time_point const& now) const noexcept
    {
        if (m_taps.empty())
        {
            return false;
        }

        auto const sinceLastTap = SecondsBetween(m_taps.back(), now);

        if (m_taps.size() < 2)
        {
            // nothing to compare against yet, so only an outright pause counts
            return sinceLastTap > std::chrono::duration_cast<std::chrono::duration<double>>(RestartAfter).count();
        }

        auto const meanInterval =
            SecondsBetween(m_taps.front(), m_taps.back()) / static_cast<double>(m_taps.size() - 1);

        if (meanInterval <= 0.0)
        {
            return false;
        }

        // Judged against the tempo in hand, so hesitating at 40 BPM is not mistaken for a new
        // sequence while the same gap at 200 BPM is.
        return sinceLastTap < meanInterval * OutlierLowFactor ||
            sinceLastTap > meanInterval * OutlierHighFactor;
    }

    std::optional<double> TapTempo::Tap() noexcept
    {
        try
        {
            auto const now = std::chrono::steady_clock::now();

            if (!m_taps.empty() && IsNewSequence(now))
            {
                // Only the new tap survives, so the display holds its last value until there
                // is a fresh interval to report rather than lurching to a half-measured one.
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

            auto const beatsPerMinute = std::round(60.0 * intervals / span / Resolution) * Resolution;

            return std::clamp(beatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
}
