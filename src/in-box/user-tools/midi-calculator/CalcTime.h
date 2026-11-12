// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Windows time values. Plain C++ with no WinRT or XAML, so the unit tests compile it unchanged.
//
// MIDI timestamps are MidiClock ticks: TimestampFrequency a second, counted from when the PC
// started. FILETIME and WinRT DateTime count 100 ns from January 1 1601 UTC. .NET DateTime
// counts 100 ns from January 1 of year 1. Unix time counts from January 1 1970 UTC.

#include <sal.h>

#include <cstdint>
#include <optional>

namespace midicalc
{
    inline constexpr int64_t HundredNanosecondsPerSecond = 10'000'000;

    // January 1 1970 as a FILETIME, and January 1 1601 as .NET ticks.
    inline constexpr int64_t UnixEpochAsFileTime = 116'444'736'000'000'000;
    inline constexpr int64_t FileTimeZeroAsDotNetTicks = 504'911'232'000'000'000;

    int64_t FileTimeToDotNetTicks(_In_ int64_t fileTime) noexcept;
    int64_t DotNetTicksToFileTime(_In_ int64_t dotNetTicks) noexcept;

    double FileTimeToUnixSeconds(_In_ int64_t fileTime) noexcept;
    int64_t FileTimeToUnixMilliseconds(_In_ int64_t fileTime) noexcept;
    int64_t UnixMillisecondsToFileTime(_In_ int64_t unixMilliseconds) noexcept;
    int64_t UnixSecondsToFileTime(_In_ double unixSeconds) noexcept;

    // Ticks at a rate (ticks a second) as seconds. Zero for a rate of zero.
    double TicksToSeconds(_In_ uint64_t ticks, _In_ uint64_t ticksPerSecond) noexcept;

    // Seconds as ticks at a rate, rounded. Negative seconds give negative ticks.
    int64_t SecondsToTicks(_In_ double seconds, _In_ uint64_t ticksPerSecond) noexcept;

    // Moves a timestamp by seconds. Empty when it would go below zero.
    std::optional<uint64_t> OffsetTimestamp(_In_ uint64_t timestamp, _In_ double seconds, _In_ uint64_t ticksPerSecond) noexcept;

    // A pair of readings taken together: a MIDI timestamp and the FILETIME (UTC) at that moment.
    // Every conversion between the two goes through one.
    struct ClockPair
    {
        uint64_t Timestamp{ 0 };
        int64_t FileTime{ 0 };
        uint64_t TicksPerSecond{ 0 };
    };

    // The FILETIME a timestamp was taken at. Empty for a rate of zero.
    std::optional<int64_t> FileTimeForTimestamp(_In_ uint64_t timestamp, _In_ ClockPair const& now) noexcept;

    // The timestamp for a FILETIME. Empty when it falls before the PC started (below zero) or the
    // rate is zero.
    std::optional<uint64_t> TimestampForFileTime(_In_ int64_t fileTime, _In_ ClockPair const& now) noexcept;

    struct HoursMinutesSeconds
    {
        uint64_t Hours{ 0 };
        uint32_t Minutes{ 0 };
        double Seconds{ 0.0 };
    };

    // For "80 h 21 min 11.48 s". Negative seconds count as zero.
    HoursMinutesSeconds SplitSeconds(_In_ double seconds) noexcept;
}
