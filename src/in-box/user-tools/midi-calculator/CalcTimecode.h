// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Timecode at the frame rates video and music apps use, and the sample and buffer math that goes
// with it. Plain C++ with no WinRT or XAML, so the unit tests compile it unchanged.
//
// MIDI Time Code (midi-app-shared\MidiTimeCode.h) carries only four of these rates, so this has
// its own math. The unit tests check the two agree at the four rates they share.
//
// Drop frame counts the nominal rate (30 or 60 frames a second) and skips frame numbers: 2 at
// 29.97 and 4 at 59.94, at the top of every minute except every tenth minute. That keeps the
// count in step with the clock on the wall. Nothing is skipped at the other rates, so 23.976 and
// non-drop 29.97 and 59.94 run slower than the clock.

#include <sal.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace midicalc
{
    // The value is saved, so only add.
    enum class FrameRate : int32_t
    {
        Fps23976 = 0,
        Fps24 = 1,
        Fps25 = 2,
        Fps2997Drop = 3,
        Fps2997 = 4,
        Fps30 = 5,
        Fps50 = 6,
        Fps5994Drop = 7,
        Fps5994 = 8,
        Fps60 = 9
    };

    inline constexpr int32_t FrameRateCount = 10;
    inline constexpr FrameRate DefaultFrameRate = FrameRate::Fps24;

    // The frame numbers counted in a second: 24 for 23.976, 30 for 29.97, 60 for 59.94.
    int32_t NominalFramesPerSecond(_In_ FrameRate rate) noexcept;

    // Frames in a real second: 24000/1001 for 23.976, and so on.
    double FramesPerSecond(_In_ FrameRate rate) noexcept;

    bool IsDropFrame(_In_ FrameRate rate) noexcept;

    struct Timecode
    {
        bool Negative{ false };
        uint32_t Hours{ 0 };
        uint32_t Minutes{ 0 };
        uint32_t Seconds{ 0 };
        uint32_t Frames{ 0 };
    };

    // Hours run up to this; past a day is fine, since a length can be longer than one.
    inline constexpr uint32_t MaximumTimecodeHours = 999;

    // A drop frame label that isn't counted, such as 00:01:00;00 at 29.97.
    bool IsSkippedFrameNumber(_In_ Timecode const& timecode, _In_ FrameRate rate) noexcept;

    // Minutes and seconds under 60, frames under the nominal rate, and not a skipped number.
    bool IsValidTimecode(_In_ Timecode const& timecode, _In_ FrameRate rate) noexcept;

    // Frames from 00:00:00:00, negative for a negative timecode.
    int64_t TimecodeToFrames(_In_ Timecode const& timecode, _In_ FrameRate rate) noexcept;
    Timecode FramesToTimecode(_In_ int64_t frames, _In_ FrameRate rate) noexcept;

    // "01:00:00:00", or "01:00:00;00" for drop frame, which is how editors write it. A minus sign
    // in front of a negative one.
    std::wstring FormatTimecode(_In_ Timecode const& timecode, _In_ FrameRate rate);

    // Accepts ":", ";" or "." between the fields. Fewer than four fields fill from the right, so
    // "1:20" is one second and twenty frames. Digits on their own fill two at a time from the
    // right, so "1000" is 00:00:10:00, as in most editors. Empty when it isn't a valid timecode at
    // the rate, including a skipped drop frame number.
    std::optional<Timecode> ParseTimecode(_In_ std::wstring_view text, _In_ FrameRate rate) noexcept;

    // Real seconds for a frame count.
    double FramesToSeconds(_In_ int64_t frames, _In_ FrameRate rate) noexcept;

    // The same moment at another frame rate: the whole frames, and the part of a frame left over
    // (0 up to but not including 1).
    struct FrameMoment
    {
        int64_t Frames{ 0 };
        double Leftover{ 0.0 };
    };

    FrameMoment SecondsToFrames(_In_ double seconds, _In_ FrameRate rate) noexcept;
    FrameMoment ConvertFrames(_In_ int64_t frames, _In_ FrameRate from, _In_ FrameRate to) noexcept;

    // Where a moment falls at a tempo and meter, counted from bar 1, beat 1 the way music apps
    // do: bar 1,800, beat 4 and 0.99 of a beat. A beat is the meter's bottom note.
    struct BarPosition
    {
        uint64_t Bar{ 1 };
        uint32_t Beat{ 1 };
        double BeatFraction{ 0.0 };
    };

    std::optional<BarPosition> BarPositionAt(_In_ double seconds, _In_ double beatsPerMinute, _In_ int32_t meterNumerator, _In_ int32_t meterDenominator) noexcept;

    // One buffer's length. Zero for a rate of zero.
    double BufferMilliseconds(_In_ uint32_t bufferSamples, _In_ double sampleRate) noexcept;
}
