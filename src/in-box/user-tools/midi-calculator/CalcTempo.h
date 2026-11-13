// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Tempo, note lengths and the beat calculator. Plain C++ with no WinRT or XAML, so the unit
// tests compile it unchanged.
//
// Tempo is always quarter notes a minute, the way music apps show it, whatever the meter. A
// "beat" in the beat calculator is one note of the meter's bottom number, so a bar of 6/8 is
// six eighth notes, which is three quarter notes.

#include <sal.h>

#include <cstdint>
#include <optional>

namespace midicalc
{
    inline constexpr double MinimumTempo = 1.0;
    inline constexpr double MaximumTempo = 1000.0;

    struct Meter
    {
        int32_t Numerator{ 4 };
        int32_t Denominator{ 4 };
    };

    // 1 to 32 over 1, 2, 4, 8, 16, 32 or 64.
    bool IsValidMeter(_In_ Meter const& meter) noexcept;

    double QuarterNotesPerBar(_In_ Meter const& meter) noexcept;

    // Bars plus beats of the meter's bottom note, in quarter notes.
    double QuarterNotesIn(_In_ double bars, _In_ double beats, _In_ Meter const& meter) noexcept;

    // The tempo that makes quarterNotes last seconds. Empty when either is zero or less, or the
    // answer is outside MinimumTempo to MaximumTempo.
    std::optional<double> TempoForLength(_In_ double quarterNotes, _In_ double seconds) noexcept;

    // How long quarterNotes last at a tempo. Empty for a tempo out of range.
    std::optional<double> SecondsForLength(_In_ double quarterNotes, _In_ double beatsPerMinute) noexcept;

    // How many bars (with a fraction) fit in seconds at a tempo.
    std::optional<double> BarsInLength(_In_ double seconds, _In_ double beatsPerMinute, _In_ Meter const& meter) noexcept;

    // ---- note lengths ----

    // The value is the note's fraction of a whole note: 1, 2, 4 ... 64.
    enum class NoteValue : int32_t
    {
        Whole = 1,
        Half = 2,
        Quarter = 4,
        Eighth = 8,
        Sixteenth = 16,
        ThirtySecond = 32,
        SixtyFourth = 64
    };

    enum class NoteFeel : int32_t
    {
        Straight = 0,
        Dotted = 1,
        Triplet = 2
    };

    double NoteQuarterNotes(_In_ NoteValue value, _In_ NoteFeel feel) noexcept;

    // Zero for a tempo out of range.
    double NoteMilliseconds(_In_ NoteValue value, _In_ NoteFeel feel, _In_ double beatsPerMinute) noexcept;

    // One cycle a note long, as a rate. Zero for zero.
    double MillisecondsToHertz(_In_ double milliseconds) noexcept;

    double MillisecondsToSamples(_In_ double milliseconds, _In_ double sampleRate) noexcept;
    double SamplesToMilliseconds(_In_ double samples, _In_ double sampleRate) noexcept;

    // ---- tempo change ----

    struct TempoChange
    {
        // 1.0667 for 120 to 128 BPM
        double SpeedRatio{ 1.0 };
        // 0.9375 for 120 to 128 BPM: a length at the new tempo is this times the old one
        double LengthRatio{ 1.0 };
        // +6.667 for 120 to 128 BPM
        double Percent{ 0.0 };
        // what speeding up the audio without time stretching does to its pitch
        double Semitones{ 0.0 };
    };

    std::optional<TempoChange> ChangeTempo(_In_ double fromBeatsPerMinute, _In_ double toBeatsPerMinute) noexcept;

    // The new tempo for a change of percent. Empty when it lands out of range.
    std::optional<double> TempoAfterPercent(_In_ double fromBeatsPerMinute, _In_ double percent) noexcept;
}
