// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. Which note a fret plays, where the frets are drawn and
// which strings a strum crosses is arithmetic, and a fretboard a semitone out is no use to
// anybody, so it is tested as arithmetic.

#include <sal.h>
#include <cstdint>
#include <span>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    bool IsFretboard(_In_ ControlKind kind) noexcept;

    // The strings of a named tuning, top to bottom as drawn: the highest string first for a
    // guitar, so the low string ends up at the bottom. Custom has none.
    std::vector<FretString> TuningStrings(_In_ FretTuning tuning);

    // The named tuning these strings are, or Custom.
    FretTuning MatchingTuning(_In_ std::vector<FretString> const& strings) noexcept;

    // The resource key for a tuning's name, so the inspector, a picker and a readout all print
    // the same name.
    wchar_t const* TuningNameResourceKey(_In_ FretTuning tuning) noexcept;

    // A fretboard with every number inside its limits, at least one string, and the frets
    // showing inside the neck. What a file from a stranger becomes before anything reads it.
    FretboardSpec NormalizedFretboard(_In_ FretboardSpec spec);

    // The note one string plays at one fret, or -1 when nothing does: behind the capo, on the
    // part of the neck a short string doesn't reach, or off either end of the note range. Fret
    // 0 is the open string, which plays at the capo.
    int32_t FretNote(_In_ FretboardSpec const& spec, _In_ int32_t string, _In_ int32_t fret) noexcept;

    // Whether a fretboard shows a key.
    bool HasFretKey(_In_ FretboardSpec const& spec) noexcept;

    // Whether a note is played: it exists, and out-of-key frets are not muted or it is in key.
    bool FretPlays(_In_ FretboardSpec const& spec, _In_ int32_t note) noexcept;

    // Where everything sits in a control of a given size.
    struct FretboardLayout
    {
        double Width{ 0.0 };
        double Height{ 0.0 };

        // The nut is on the right and the frets count up to the left. Set when the strum area
        // is on the left, for a left-handed player.
        bool Mirrored{ false };

        bool HasStrum{ false };
        double StrumLeft{ 0.0 };
        double StrumRight{ 0.0 };

        // The column that plays each string open, beside the nut.
        double OpenLeft{ 0.0 };
        double OpenRight{ 0.0 };

        // The fretted part of the neck that shows.
        double NeckLeft{ 0.0 };
        double NeckRight{ 0.0 };

        // A strip under the neck showing the whole neck and which part of it shows. Dragged to
        // move along the neck.
        bool HasStrip{ false };
        double StripTop{ 0.0 };
        double StripBottom{ 0.0 };

        double StringsTop{ 0.0 };
        double StringsBottom{ 0.0 };

        // From one string to the next.
        double StringPitch{ 0.0 };

        // Each string's line, in list order: top to bottom.
        std::vector<double> StringY{};

        // The frets that show, as the spaces between wires. Fret 1 is the space between the nut
        // and the first wire.
        int32_t FirstCell{ 1 };
        int32_t LastCell{ 12 };

        // Where each wire is drawn: WireX[0] is the wire before FirstCell (the nut, when
        // FirstCell is 1), and the last is the wire after LastCell.
        std::vector<double> WireX{};
    };

    FretboardLayout LayOutFretboard(_In_ FretboardSpec const& spec, _In_ double width, _In_ double height);

    // The middle of a fret, for drawing a dot or a finger on it. Fret 0 is the open column.
    double FretCenterX(_In_ FretboardLayout const& layout, _In_ int32_t fret) noexcept;

    enum class FretRegion
    {
        None = 0,
        Open = 1,
        Neck = 2,
        Strum = 3,
        Strip = 4,
    };

    struct FretHit
    {
        FretRegion Region{ FretRegion::None };

        // The nearest string, in list order. Every point between two strings belongs to one of
        // them, so a finger never lands on nothing.
        int32_t String{ -1 };

        // The fret under the point: 0 in the open column.
        int32_t Fret{ 0 };

        // The same, smoothly: the middle of a fret is its own number, and a point between two
        // middles is part way between their numbers. What a fretless slide follows.
        double FretPosition{ 0.0 };
    };

    FretHit FretAtPoint(_In_ FretboardLayout const& layout, _In_ double x, _In_ double y) noexcept;

    // The fret under a point along the neck, wherever it is across the strings.
    int32_t FretAtX(_In_ FretboardLayout const& layout, _In_ double x) noexcept;
    double FretPositionAtX(_In_ FretboardLayout const& layout, _In_ double x) noexcept;

    int32_t StringAtY(_In_ FretboardLayout const& layout, _In_ double y) noexcept;

    // The strings whose lines lie past y0 and up to y1, in the order a finger moving from one
    // to the other crosses them: downward on the screen crosses them top first. Returns how many
    // were written.
    uint32_t StringsCrossed(
        _In_ FretboardLayout const& layout,
        _In_ double y0,
        _In_ double y1,
        _Out_writes_to_(out.size(), return) std::span<int32_t> out) noexcept;

    // How close to a string a press in the strum area has to land to pluck it, as a fraction of
    // the distance between strings.
    constexpr double PluckReach = 0.25;

    // How hard a strum plays, 0 to 1, from how far the finger went across the strings and how
    // long it took. A slow brush is soft and a fast sweep is loud.
    double StrumVelocity(_In_ double distance, _In_ double milliseconds) noexcept;

    // How far across its string a finger has to move before the string starts to bend, and how
    // far for the whole bend range, as fractions of the distance between strings. A finger
    // resting on a fret has to be in tune.
    constexpr double BendDeadZone = 0.1;
    constexpr double BendFullReach = 1.0;

    // The bend, in semitones, for a finger this far across its string from where it landed.
    // Either way bends up, the way a real string only ever bends sharp.
    double CrossBend(_In_ int32_t bendRangeSemitones, _In_ double across, _In_ double stringPitch) noexcept;

    // The bend, in semitones, for a finger that struck one fret and has slid to another.
    double SlideBend(
        _In_ FretSlide slide,
        _In_ int32_t struckFret,
        _In_ int32_t fret,
        _In_ double fretPosition) noexcept;

    // The first fret to show for a press on the neck strip, so the frets showing center on it.
    // The number showing never changes. Never less than 1: frets 0 and 1 both start at the nut.
    int32_t NeckWindowAt(
        _In_ FretboardSpec const& spec,
        _In_ FretboardLayout const& layout,
        _In_ double x) noexcept;

    // The fretboard with the frets showing moved to start at this fret, keeping how many show,
    // and kept on the neck.
    FretboardSpec WithNeckAt(_In_ FretboardSpec spec, _In_ int32_t firstCell) noexcept;

    // Where on the strip a fret is, for drawing the whole neck in miniature.
    double StripX(_In_ FretboardSpec const& spec, _In_ FretboardLayout const& layout, _In_ double fret) noexcept;

    // Frets that get an inlay dot on a real neck: 3, 5, 7, 9, and two dots at 12, and the same
    // again an octave up.
    int32_t InlayDots(_In_ int32_t fret) noexcept;
}
