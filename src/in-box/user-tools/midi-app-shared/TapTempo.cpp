// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "TapTempo.h"

#include <algorithm>
#include <cmath>

namespace midiapp
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

    _Use_decl_annotations_
    TapTempo::TapTempo(double minimumBeatsPerMinute, double maximumBeatsPerMinute, double resolution) noexcept :
        m_minimum{ minimumBeatsPerMinute },
        m_maximum{ (std::max)(minimumBeatsPerMinute, maximumBeatsPerMinute) },
        m_resolution{ resolution > 0.0 ? resolution : 0.0 }
    {
    }

    void TapTempo::Reset() noexcept
    {
        m_taps.clear();
    }

    _Use_decl_annotations_
    bool TapTempo::IsNewSequence(std::chrono::steady_clock::time_point const& now) const noexcept
    {
        if (m_taps.empty())
        {
            return false;
        }

        auto const sinceLastTap = SecondsBetween(m_taps.back(), now);

        if (sinceLastTap < 0.0)
        {
            // a timestamp from before the last tap can't continue anything
            return true;
        }

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
        return TapAt(std::chrono::steady_clock::now());
    }

    _Use_decl_annotations_
    std::optional<double> TapTempo::TapAt(std::chrono::steady_clock::time_point const& when) noexcept
    {
        try
        {
            if (!m_taps.empty() && IsNewSequence(when))
            {
                // Only the new tap survives, so the display holds its last value until there
                // is a fresh interval to report rather than lurching to a half-measured one.
                m_taps.clear();
            }

            m_taps.push_back(when);

            if (m_taps.size() > MaximumTapsConsidered)
            {
                m_taps.erase(m_taps.begin(), m_taps.end() - MaximumTapsConsidered);
            }

            if (m_taps.size() < 2)
            {
                return std::nullopt;
            }

            auto const span = SecondsBetween(m_taps.front(), m_taps.back());
            auto const intervals = static_cast<double>(m_taps.size() - 1);

            if (span <= 0.0)
            {
                return std::nullopt;
            }

            auto beatsPerMinute = 60.0 * intervals / span;

            if (m_resolution > 0.0)
            {
                beatsPerMinute = std::round(beatsPerMinute / m_resolution) * m_resolution;
            }

            return std::clamp(beatsPerMinute, m_minimum, m_maximum);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<double> TapTempo::SpreadMilliseconds() const noexcept
    {
        if (m_taps.size() < 3)
        {
            return std::nullopt;
        }

        auto const mean = SecondsBetween(m_taps.front(), m_taps.back()) / static_cast<double>(m_taps.size() - 1);
        double largest{ 0.0 };

        for (size_t i = 1; i < m_taps.size(); ++i)
        {
            largest = (std::max)(largest, std::abs(SecondsBetween(m_taps[i - 1], m_taps[i]) - mean));
        }

        return largest * 1000.0;
    }
}
