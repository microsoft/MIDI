// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK.

#include "ClockTempo.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    _Use_decl_annotations_
    std::optional<double> ClockTempoMeter::Tick(uint64_t microseconds) noexcept
    {
        if (m_count > 0)
        {
            auto const newest = m_times[(m_next + m_times.size() - 1) % m_times.size()];

            // Time going backwards, or a pause long enough to be a stop, starts the count again.
            // The same time twice is two messages that arrived together, which is not either.
            if (microseconds < newest || microseconds - newest > ClockGapMicroseconds)
            {
                m_count = 0;
                m_next = 0;
            }
        }

        m_times[m_next] = microseconds;
        m_next = (m_next + 1) % m_times.size();
        m_count = (std::min)(m_count + 1, m_times.size());

        if (m_count < MinimumClockTicksForTempo)
        {
            return std::nullopt;
        }

        auto const oldest = m_times[(m_next + m_times.size() - m_count) % m_times.size()];
        auto const intervals = static_cast<double>(m_count - 1);
        auto const span = static_cast<double>(microseconds - oldest);

        if (span <= 0.0)
        {
            return std::nullopt;
        }

        auto const microsecondsPerQuarter = span / intervals * ClockTicksPerQuarterNote;

        auto const measured = std::clamp(
            60'000'000.0 / microsecondsPerQuarter,
            MinimumBeatsPerMinute,
            MaximumBeatsPerMinute);

        if (m_published > 0.0 && std::abs(measured - m_published) < ClockTempoHysteresis)
        {
            return std::nullopt;
        }

        m_published = std::round(measured * 10.0) / 10.0;

        return m_published;
    }

    void ClockTempoMeter::Reset() noexcept
    {
        m_times.fill(0);
        m_count = 0;
        m_next = 0;
        m_published = 0.0;
    }
}
