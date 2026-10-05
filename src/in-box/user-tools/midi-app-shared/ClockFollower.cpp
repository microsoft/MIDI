// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "ClockFollower.h"

#include <algorithm>

namespace midiapp
{
    namespace
    {
        bool Near(_In_ uint64_t value, _In_ uint64_t reference) noexcept
        {
            return (value > reference ? value - reference : reference - value) <= reference / 2;
        }
    }

    _Use_decl_annotations_
    ClockFollower::Mark const& ClockFollower::At(size_t index) const noexcept
    {
        return m_marks[(m_first + index) % Capacity];
    }

    _Use_decl_annotations_
    void ClockFollower::Measure(uint64_t gap) noexcept
    {
        if (gap == 0)
        {
            return;
        }

        if (m_interval == 0)
        {
            m_interval = gap;
        }
        else if (Near(gap, m_interval))
        {
            // Smoothed, so the jitter a connection puts on each pulse doesn't reach the pace.
            auto const difference = static_cast<int64_t>(gap) - static_cast<int64_t>(m_interval);
            m_interval = static_cast<uint64_t>(static_cast<int64_t>(m_interval) + difference / 4);
        }
        else if (m_lastGap != 0 && Near(gap, m_lastGap))
        {
            // The same new gap twice is a new tempo. Once is a pause or a hiccup.
            m_interval = gap;
        }

        m_lastGap = gap;
    }

    _Use_decl_annotations_
    void ClockFollower::Pulse(uint64_t time) noexcept
    {
        if (m_count > 0)
        {
            auto const last = At(m_count - 1).Time;

            // Two clocks at once can arrive out of order. Kept in order rather than trusted.
            time = (std::max)(time, last);

            Measure(time - last);
        }

        if (m_count == Capacity)
        {
            m_first = (m_first + 1) % Capacity;
            m_count--;
        }

        m_marks[(m_first + m_count) % Capacity] = Mark{ time, m_nextNumber++ };
        m_count++;
    }

    _Use_decl_annotations_
    void ClockFollower::Restart(uint64_t time, uint64_t nextNumber) noexcept
    {
        // Pulses handed over early for after this point belong to the run it replaces.
        while (m_count > 0 && At(m_count - 1).Time >= time)
        {
            m_count--;
        }

        m_nextNumber = nextNumber;
    }

    _Use_decl_annotations_
    void ClockFollower::Start(uint64_t time) noexcept
    {
        Restart(time, 0);
    }

    _Use_decl_annotations_
    void ClockFollower::SongPosition(uint64_t time, uint32_t sixteenths) noexcept
    {
        Restart(time, static_cast<uint64_t>(sixteenths) * MidiClocksPerSongPositionStep);
    }

    _Use_decl_annotations_
    std::optional<double> ClockFollower::PulsesAt(uint64_t time) const noexcept
    {
        // The first pulse after this time.
        size_t low{ 0 };
        size_t high{ m_count };

        while (low < high)
        {
            auto const middle = low + (high - low) / 2;

            if (At(middle).Time <= time)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }

        if (low == 0)
        {
            return std::nullopt;
        }

        auto const& mark = At(low - 1);
        auto const elapsed = static_cast<double>(time - mark.Time);

        double fraction{ 0.0 };

        if (low < m_count && At(low).Number == mark.Number + 1 && At(low).Time > mark.Time)
        {
            fraction = elapsed / static_cast<double>(At(low).Time - mark.Time);
        }
        else if (m_interval > 0)
        {
            fraction = elapsed / static_cast<double>(m_interval);
        }

        return static_cast<double>(mark.Number) + (std::min)(fraction, 1.0);
    }

    uint64_t ClockFollower::KnownUntil() const noexcept
    {
        return m_count == 0 ? 0 : At(m_count - 1).Time + m_interval;
    }
}
