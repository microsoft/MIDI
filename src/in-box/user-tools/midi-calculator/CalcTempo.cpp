// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "CalcTempo.h"

#include <cmath>

namespace midicalc
{
    namespace
    {
        bool IsTempoInRange(_In_ double beatsPerMinute) noexcept
        {
            return std::isfinite(beatsPerMinute) && beatsPerMinute >= MinimumTempo && beatsPerMinute <= MaximumTempo;
        }
    }

    _Use_decl_annotations_
    bool IsValidMeter(Meter const& meter) noexcept
    {
        if (meter.Numerator < 1 || meter.Numerator > 32)
        {
            return false;
        }

        switch (meter.Denominator)
        {
        case 1: case 2: case 4: case 8: case 16: case 32: case 64:
            return true;
        default:
            return false;
        }
    }

    _Use_decl_annotations_
    double QuarterNotesPerBar(Meter const& meter) noexcept
    {
        if (!IsValidMeter(meter))
        {
            return 0.0;
        }

        return static_cast<double>(meter.Numerator) * 4.0 / static_cast<double>(meter.Denominator);
    }

    _Use_decl_annotations_
    double QuarterNotesIn(double bars, double beats, Meter const& meter) noexcept
    {
        if (!IsValidMeter(meter))
        {
            return 0.0;
        }

        return bars * QuarterNotesPerBar(meter) + beats * 4.0 / static_cast<double>(meter.Denominator);
    }

    _Use_decl_annotations_
    std::optional<double> TempoForLength(double quarterNotes, double seconds) noexcept
    {
        if (!(quarterNotes > 0.0) || !(seconds > 0.0))
        {
            return std::nullopt;
        }

        auto const tempo = 60.0 * quarterNotes / seconds;

        if (!IsTempoInRange(tempo))
        {
            return std::nullopt;
        }

        return tempo;
    }

    _Use_decl_annotations_
    std::optional<double> SecondsForLength(double quarterNotes, double beatsPerMinute) noexcept
    {
        if (!IsTempoInRange(beatsPerMinute) || !(quarterNotes >= 0.0))
        {
            return std::nullopt;
        }

        return quarterNotes * 60.0 / beatsPerMinute;
    }

    _Use_decl_annotations_
    std::optional<double> BarsInLength(double seconds, double beatsPerMinute, Meter const& meter) noexcept
    {
        if (!IsTempoInRange(beatsPerMinute) || !IsValidMeter(meter) || !(seconds >= 0.0))
        {
            return std::nullopt;
        }

        return seconds * beatsPerMinute / 60.0 / QuarterNotesPerBar(meter);
    }

    _Use_decl_annotations_
    double NoteQuarterNotes(NoteValue value, NoteFeel feel) noexcept
    {
        auto const division = static_cast<int32_t>(value);

        if (division < 1)
        {
            return 0.0;
        }

        auto quarters = 4.0 / static_cast<double>(division);

        switch (feel)
        {
        case NoteFeel::Dotted:
            quarters *= 1.5;
            break;
        case NoteFeel::Triplet:
            quarters *= 2.0 / 3.0;
            break;
        default:
            break;
        }

        return quarters;
    }

    _Use_decl_annotations_
    double NoteMilliseconds(NoteValue value, NoteFeel feel, double beatsPerMinute) noexcept
    {
        if (!IsTempoInRange(beatsPerMinute))
        {
            return 0.0;
        }

        return NoteQuarterNotes(value, feel) * 60000.0 / beatsPerMinute;
    }

    _Use_decl_annotations_
    double MillisecondsToHertz(double milliseconds) noexcept
    {
        return milliseconds > 0.0 ? 1000.0 / milliseconds : 0.0;
    }

    _Use_decl_annotations_
    double MillisecondsToSamples(double milliseconds, double sampleRate) noexcept
    {
        return milliseconds * sampleRate / 1000.0;
    }

    _Use_decl_annotations_
    double SamplesToMilliseconds(double samples, double sampleRate) noexcept
    {
        return sampleRate > 0.0 ? samples * 1000.0 / sampleRate : 0.0;
    }

    _Use_decl_annotations_
    std::optional<TempoChange> ChangeTempo(double fromBeatsPerMinute, double toBeatsPerMinute) noexcept
    {
        if (!IsTempoInRange(fromBeatsPerMinute) || !IsTempoInRange(toBeatsPerMinute))
        {
            return std::nullopt;
        }

        TempoChange change{};
        change.SpeedRatio = toBeatsPerMinute / fromBeatsPerMinute;
        change.LengthRatio = fromBeatsPerMinute / toBeatsPerMinute;
        change.Percent = (change.SpeedRatio - 1.0) * 100.0;
        change.Semitones = 12.0 * std::log2(change.SpeedRatio);

        return change;
    }

    _Use_decl_annotations_
    std::optional<double> TempoAfterPercent(double fromBeatsPerMinute, double percent) noexcept
    {
        if (!IsTempoInRange(fromBeatsPerMinute))
        {
            return std::nullopt;
        }

        auto const tempo = fromBeatsPerMinute * (1.0 + percent / 100.0);

        if (!IsTempoInRange(tempo))
        {
            return std::nullopt;
        }

        return tempo;
    }
}
