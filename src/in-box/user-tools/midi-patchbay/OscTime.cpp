// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "OscTime.h"
#include "OscPacket.h"

#include <algorithm>

namespace midipatchbay::osc
{
    namespace
    {
        constexpr uint64_t SecondsPerEra = 1ull << 32;
        constexpr int64_t SecondsPerDay = 86'400;
        constexpr int64_t FileTimePerDay = SecondsPerDay * static_cast<int64_t>(FileTimeTicksPerSecond);

        // a * b / c without overflow, for a up to a day of either unit and b, c up to tick rates.
        int64_t Scale(_In_ int64_t value, _In_ int64_t from, _In_ int64_t to) noexcept
        {
            return (value / from) * to + ((value % from) * to) / from;
        }

        // A difference between two time tags (seconds in the high 32 bits, 2^-32 seconds in the
        // low 32) in ticks. Split so nothing overflows for tick rates below 2^40 per second.
        int64_t TicksFromTimeTagDifference(_In_ int64_t difference, _In_ uint64_t ticksPerSecond) noexcept
        {
            auto const negative = difference < 0;
            auto const size = negative ? 0ull - static_cast<uint64_t>(difference) : static_cast<uint64_t>(difference);

            auto const whole = (size >> 32) * ticksPerSecond;
            auto const fraction = size & 0xFFFF'FFFFull;
            auto const part = (((fraction >> 16) * ticksPerSecond) >> 16) + (((fraction & 0xFFFFull) * ticksPerSecond) >> 32);

            auto const ticks = static_cast<int64_t>(whole + part);

            return negative ? -ticks : ticks;
        }
    }

    _Use_decl_annotations_
    uint64_t FileTimeFromTimeTag(uint64_t timeTag) noexcept
    {
        auto seconds = timeTag >> 32;
        auto const fraction = timeTag & 0xFFFF'FFFFull;

        if (seconds < (1ull << 31))
        {
            seconds += SecondsPerEra;
        }

        return (seconds + SecondsFrom1601To1900) * FileTimeTicksPerSecond +
            ((fraction * FileTimeTicksPerSecond) >> 32);
    }

    _Use_decl_annotations_
    uint64_t TimeTagFromFileTime(uint64_t fileTime) noexcept
    {
        auto const wholeSeconds = fileTime / FileTimeTicksPerSecond;

        if (wholeSeconds < SecondsFrom1601To1900)
        {
            return 0;
        }

        auto const seconds = (wholeSeconds - SecondsFrom1601To1900) % SecondsPerEra;
        auto const remainder = fileTime % FileTimeTicksPerSecond;

        // Rounded up, so reading the tag back gives exactly this FILETIME.
        auto const fraction = ((remainder << 32) + FileTimeTicksPerSecond - 1) / FileTimeTicksPerSecond;

        return (seconds << 32) | fraction;
    }

    _Use_decl_annotations_
    uint64_t TicksFromTimeTag(uint64_t timeTag, ClockPair const& clock) noexcept
    {
        if (timeTag <= 1 || clock.TicksPerSecond == 0)
        {
            return clock.Ticks;
        }

        auto const fileTime = FileTimeFromTimeTag(timeTag);
        auto const difference = std::clamp(
            static_cast<int64_t>(fileTime - clock.FileTime), -FileTimePerDay, FileTimePerDay);

        auto const ticks = Scale(difference, static_cast<int64_t>(FileTimeTicksPerSecond), static_cast<int64_t>(clock.TicksPerSecond));

        if (ticks < 0 && static_cast<uint64_t>(-ticks) > clock.Ticks)
        {
            return 0;
        }

        return clock.Ticks + static_cast<uint64_t>(ticks);
    }

    _Use_decl_annotations_
    uint64_t TimeTagFromTicks(uint64_t ticks, ClockPair const& clock) noexcept
    {
        if (clock.TicksPerSecond == 0)
        {
            return Immediately;
        }

        auto const tickRate = static_cast<int64_t>(clock.TicksPerSecond);
        auto const difference = std::clamp(
            static_cast<int64_t>(ticks - clock.Ticks), -SecondsPerDay * tickRate, SecondsPerDay * tickRate);

        auto const fileTime = clock.FileTime +
            static_cast<uint64_t>(Scale(difference, tickRate, static_cast<int64_t>(FileTimeTicksPerSecond)));

        return TimeTagFromFileTime(fileTime);
    }

    _Use_decl_annotations_
    uint64_t SpacingKeeper::PlayAt(uint64_t timeTag, uint64_t arrivalTicks, uint64_t delayTicks, uint64_t ticksPerSecond) noexcept
    {
        auto const window = ticksPerSecond * WindowMilliseconds / 1000;
        auto fresh = !m_hasCurrent && !m_hasPrevious;

        if (!fresh && arrivalTicks >= m_windowStart && arrivalTicks - m_windowStart >= window)
        {
            // Two windows or more since the last message: nothing learned is current any more.
            auto const twoWindows = arrivalTicks - m_windowStart >= 2 * window;

            m_previousSmallest = m_currentSmallest;
            m_hasPrevious = m_hasCurrent && !twoWindows;
            m_hasCurrent = false;
            m_windowStart = arrivalTicks;

            fresh = !m_hasPrevious;
        }

        if (fresh)
        {
            m_windowStart = arrivalTicks;
            m_anchorTag = timeTag;
            m_anchorTicks = static_cast<int64_t>(arrivalTicks);
        }

        // NTP differences wrap with the 32-bit seconds count, so this is right within 68 years
        // of the anchor, whatever era either clock thinks it is in.
        auto const fromAnchor = static_cast<int64_t>(timeTag - m_anchorTag);
        auto const tagTicks = m_anchorTicks + TicksFromTimeTagDifference(fromAnchor, ticksPerSecond);

        auto const gap = static_cast<int64_t>(arrivalTicks) - tagTicks;

        m_currentSmallest = m_hasCurrent ? (std::min)(m_currentSmallest, gap) : gap;
        m_hasCurrent = true;

        auto const smallest = m_hasPrevious ? (std::min)(m_currentSmallest, m_previousSmallest) : m_currentSmallest;

        auto const playAt = tagTicks + smallest + static_cast<int64_t>(delayTicks);

        return playAt < static_cast<int64_t>(arrivalTicks) ? arrivalTicks : static_cast<uint64_t>(playAt);
    }

    void SpacingKeeper::Reset() noexcept
    {
        *this = SpacingKeeper{};
    }
}
