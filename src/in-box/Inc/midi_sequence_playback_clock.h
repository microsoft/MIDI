// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace midiplayer
{
    // Where a sequence's own time falls on the MIDI clock, and how fast it plays.
    //
    // Messages are handed to the service ahead of when they are due, and once handed over they
    // cannot be recalled. So a new rate cannot start now: what was handed over at the old rate
    // would overlap what is handed over next, and a note could end before it starts. A new rate
    // starts instead at the end of what has been handed over, and until the clock gets there the
    // position is still worked out at the old rate.
    //
    // Only one change waits at a time. A request made while one is waiting is held until the
    // clock reaches the waiting one, and a newer request replaces an older held one. That is what
    // keeps the position exact: the stretch before a change is always at one known rate.
    //
    // Not thread safe. The playback engine only calls it with its own lock held.
    class PlaybackClock
    {
    public:
        static constexpr double MinimumRate = 0.25;
        static constexpr double MaximumRate = 2.0;

        // Starts the clock at a point in the sequence. The rate last requested applies straight
        // away, because nothing has been handed over yet.
        void Start(
            _In_ uint64_t timestampFrequency,
            _In_ uint64_t originTimestamp,
            _In_ uint64_t originMicroseconds) noexcept
        {
            m_frequency = timestampFrequency;
            m_originTimestamp = originTimestamp;
            m_originMicroseconds = originMicroseconds;
            m_scheduledThroughTimestamp = originTimestamp;
            m_rate = m_requestedRate;

            // Nothing plays before the origin of a fresh start, so the position holds there.
            m_leadInRate = 0.0;
        }

        // Clamps the rate to the supported range. A value that is not a number changes nothing
        // and returns false.
        bool RequestRate(_In_ double rate) noexcept
        {
            if (!std::isfinite(rate))
            {
                return false;
            }

            m_requestedRate = std::clamp(rate, MinimumRate, MaximumRate);

            return true;
        }

        double RequestedRate() const noexcept { return m_requestedRate; }
        double Rate() const noexcept { return m_rate; }

        // Both values are only ever copied from one another, so comparing them exactly is safe.
        bool IsRateChangePending() const noexcept { return m_requestedRate != m_rate; }

        // Everything due at or before this timestamp has been handed over.
        void MarkScheduledThrough(_In_ uint64_t timestamp) noexcept
        {
            if (timestamp > m_scheduledThroughTimestamp)
            {
                m_scheduledThroughTimestamp = timestamp;
            }
        }

        // Moves the clock to the requested rate if it can move yet. Call it before handing over
        // anything new. True when the rate changed.
        bool ApplyRequestedRate(_In_ uint64_t nowTimestamp) noexcept
        {
            if (!IsRateChangePending())
            {
                return false;
            }

            // An earlier change is still waiting for the clock to reach it.
            if (nowTimestamp < m_originTimestamp)
            {
                return false;
            }

            auto const start = (std::max)(nowTimestamp, m_scheduledThroughTimestamp);

            if (start != m_originTimestamp)
            {
                m_originMicroseconds = MicrosecondsAtTimestamp(start);
                m_originTimestamp = start;
                m_leadInRate = m_rate;
            }

            m_rate = m_requestedRate;

            return true;
        }

        // A point in the sequence before the origin plays at the origin, because it is either
        // already past or was never going to play.
        uint64_t TimestampForMicroseconds(_In_ uint64_t microseconds) const noexcept
        {
            if (microseconds <= m_originMicroseconds)
            {
                return m_originTimestamp;
            }

            auto const elapsed = (microseconds - m_originMicroseconds) * m_frequency / MicrosecondsPerSecond;

            // At the written speed this is exactly the arithmetic the engine has always used.
            if (m_rate == 1.0)
            {
                return m_originTimestamp + elapsed;
            }

            return m_originTimestamp + static_cast<uint64_t>(static_cast<double>(elapsed) / m_rate);
        }

        uint64_t MicrosecondsAtTimestamp(_In_ uint64_t timestamp) const noexcept
        {
            if (m_frequency == 0)
            {
                return m_originMicroseconds;
            }

            if (timestamp <= m_originTimestamp)
            {
                // After a start, nothing plays before the origin. After a rate change, what plays
                // until the origin was handed over at the old rate.
                if (m_leadInRate <= 0.0)
                {
                    return m_originMicroseconds;
                }

                auto const before = Scale(
                    (m_originTimestamp - timestamp) * MicrosecondsPerSecond / m_frequency,
                    m_leadInRate);

                return before < m_originMicroseconds ? m_originMicroseconds - before : 0;
            }

            auto const elapsed = (timestamp - m_originTimestamp) * MicrosecondsPerSecond / m_frequency;

            return m_originMicroseconds + Scale(elapsed, m_rate);
        }

    private:
        static constexpr uint64_t MicrosecondsPerSecond = 1000000;

        static uint64_t Scale(_In_ uint64_t microseconds, _In_ double rate) noexcept
        {
            if (rate == 1.0)
            {
                return microseconds;
            }

            return static_cast<uint64_t>(static_cast<double>(microseconds) * rate);
        }

        uint64_t m_frequency{ 0 };

        // The instant, on the MIDI clock, that m_originMicroseconds in the sequence falls at.
        uint64_t m_originTimestamp{ 0 };
        uint64_t m_originMicroseconds{ 0 };

        uint64_t m_scheduledThroughTimestamp{ 0 };

        double m_rate{ 1.0 };
        double m_requestedRate{ 1.0 };

        // The rate before m_originTimestamp. Zero after a start.
        double m_leadInRate{ 0.0 };
    };
}
