// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "CalcTime.h"

#include <cmath>

namespace midicalc
{
    _Use_decl_annotations_
    int64_t FileTimeToDotNetTicks(int64_t fileTime) noexcept
    {
        return fileTime + FileTimeZeroAsDotNetTicks;
    }

    _Use_decl_annotations_
    int64_t DotNetTicksToFileTime(int64_t dotNetTicks) noexcept
    {
        return dotNetTicks - FileTimeZeroAsDotNetTicks;
    }

    _Use_decl_annotations_
    double FileTimeToUnixSeconds(int64_t fileTime) noexcept
    {
        return static_cast<double>(fileTime - UnixEpochAsFileTime) / HundredNanosecondsPerSecond;
    }

    _Use_decl_annotations_
    int64_t FileTimeToUnixMilliseconds(int64_t fileTime) noexcept
    {
        auto const hundreds = fileTime - UnixEpochAsFileTime;

        // round toward minus infinity, so a moment before 1970 doesn't round up to a later one
        auto milliseconds = hundreds / 10'000;

        if (hundreds % 10'000 < 0)
        {
            --milliseconds;
        }

        return milliseconds;
    }

    _Use_decl_annotations_
    int64_t UnixMillisecondsToFileTime(int64_t unixMilliseconds) noexcept
    {
        return unixMilliseconds * 10'000 + UnixEpochAsFileTime;
    }

    _Use_decl_annotations_
    int64_t UnixSecondsToFileTime(double unixSeconds) noexcept
    {
        return std::llround(unixSeconds * HundredNanosecondsPerSecond) + UnixEpochAsFileTime;
    }

    _Use_decl_annotations_
    double TicksToSeconds(uint64_t ticks, uint64_t ticksPerSecond) noexcept
    {
        if (ticksPerSecond == 0)
        {
            return 0.0;
        }

        // whole seconds and the remainder apart, so a large count keeps its last digits
        return static_cast<double>(ticks / ticksPerSecond) +
            static_cast<double>(ticks % ticksPerSecond) / static_cast<double>(ticksPerSecond);
    }

    _Use_decl_annotations_
    int64_t SecondsToTicks(double seconds, uint64_t ticksPerSecond) noexcept
    {
        return std::llround(seconds * static_cast<double>(ticksPerSecond));
    }

    _Use_decl_annotations_
    std::optional<uint64_t> OffsetTimestamp(uint64_t timestamp, double seconds, uint64_t ticksPerSecond) noexcept
    {
        auto const offset = SecondsToTicks(seconds, ticksPerSecond);

        if (offset < 0 && static_cast<uint64_t>(-offset) > timestamp)
        {
            return std::nullopt;
        }

        return offset < 0 ? timestamp - static_cast<uint64_t>(-offset) : timestamp + static_cast<uint64_t>(offset);
    }

    _Use_decl_annotations_
    std::optional<int64_t> FileTimeForTimestamp(uint64_t timestamp, ClockPair const& now) noexcept
    {
        if (now.TicksPerSecond == 0)
        {
            return std::nullopt;
        }

        auto const earlier = timestamp <= now.Timestamp;
        auto const gap = earlier ? now.Timestamp - timestamp : timestamp - now.Timestamp;
        auto const hundreds = static_cast<int64_t>(std::llround(TicksToSeconds(gap, now.TicksPerSecond) * HundredNanosecondsPerSecond));

        return earlier ? now.FileTime - hundreds : now.FileTime + hundreds;
    }

    _Use_decl_annotations_
    std::optional<uint64_t> TimestampForFileTime(int64_t fileTime, ClockPair const& now) noexcept
    {
        if (now.TicksPerSecond == 0)
        {
            return std::nullopt;
        }

        auto const seconds = static_cast<double>(fileTime - now.FileTime) / HundredNanosecondsPerSecond;
        return OffsetTimestamp(now.Timestamp, seconds, now.TicksPerSecond);
    }

    _Use_decl_annotations_
    HoursMinutesSeconds SplitSeconds(double seconds) noexcept
    {
        HoursMinutesSeconds result{};

        if (!(seconds > 0.0) || !std::isfinite(seconds))
        {
            return result;
        }

        auto const whole = static_cast<uint64_t>(seconds);
        result.Hours = whole / 3600;
        result.Minutes = static_cast<uint32_t>((whole % 3600) / 60);
        result.Seconds = seconds - static_cast<double>(whole - whole % 60);

        return result;
    }
}
