// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. Where each pad sits, which note it plays and whether that
// note is in the key is arithmetic, and a grid that plays the wrong note is the kind of defect
// nobody finds until it is on stage.

#include <sal.h>
#include <cstdint>
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // A grid of pads that each play a note: square pads, or hexagons.
    bool IsPadGrid(_In_ ControlKind kind) noexcept;

    // Hexagons stand on a point and sit in rows, each row half a pad over from the one below.
    bool IsHexPadGrid(_In_ ControlKind kind) noexcept;

    struct PadCell
    {
        double CenterX{ 0.0 };
        double CenterY{ 0.0 };

        // Row 0 is the bottom row, and column 0 the left end of its row.
        int32_t Row{ 0 };
        int32_t Column{ 0 };

        // -1 where the layout runs off either end of the note range. Drawn dim, and plays
        // nothing.
        int32_t Note{ -1 };
    };

    struct PadGridLayout
    {
        bool Hex{ false };

        // The size actually drawn. That is the size asked for, unless the control is too small
        // to hold every pad at it.
        double PadWidth{ 0.0 };
        double PadHeight{ 0.0 };
        double Gap{ 0.0 };

        // Center to center, along a row and from one row to the next.
        double Pitch{ 0.0 };
        double RowPitch{ 0.0 };

        int32_t Columns{ 0 };
        int32_t Rows{ 0 };

        // In the order they flow: along the bottom row from the left, then the row above it.
        std::vector<PadCell> Cells{};
    };

    // Lays the pads out in a control of this size. As many to a row as the width holds at the
    // size asked for; when the rows that makes do not fit the height, every pad shrinks rather
    // than any of them being left off.
    PadGridLayout LayOutPadGrid(
        _In_ PadGridSpec const& spec,
        _In_ bool hex,
        _In_ double width,
        _In_ double height);

    // The largest pad that fits this many columns and rows in a control of this size, or zero
    // when not even a tiny one does.
    double PadSizeToFit(
        _In_ bool hex,
        _In_ int32_t columns,
        _In_ int32_t rows,
        _In_ double width,
        _In_ double height) noexcept;

    // The note one pad plays, or -1 when it is off either end of the note range.
    int32_t PadNote(
        _In_ PadGridSpec const& spec,
        _In_ bool hex,
        _In_ int32_t row,
        _In_ int32_t column,
        _In_ int32_t columns) noexcept;

    // Which pad a point lands on, as an index into the layout's cells, or -1. The gap between
    // two pads belongs to the nearer of them, so a finger sliding across never falls through
    // a crack between two notes.
    int32_t PadAtPoint(_In_ PadGridLayout const& layout, _In_ double x, _In_ double y) noexcept;

    // How much of a pad either side of its middle plays exactly its note, as a fraction of the
    // distance from one pad to the next. A finger resting on a pad has to be in tune.
    constexpr double PadInTuneHalfWidth = 0.2;

    // The pitch under a finger on a pad, in semitones, for a note that bends to follow it. Flat
    // across the middle of the pad, then rising to meet the next pad along the row at the edge
    // between them. Only the row decides: a finger moving onto another row lands on that row's
    // pitch.
    double PitchAtPoint(
        _In_ PadGridLayout const& layout,
        _In_ PadGridSpec const& spec,
        _In_ int32_t cell,
        _In_ double x) noexcept;

    enum class PadRole
    {
        OutOfKey = 0,
        InKey = 1,
        Root = 2,
    };

    // Whether a note is the key's root, in the key, or outside it. With no key every note is in
    // it, which is what makes every pad the same color.
    PadRole RoleOfNote(_In_ PadGridSpec const& spec, _In_ int32_t note) noexcept;

    bool IsInScale(_In_ MusicalScale scale, _In_ int32_t root, _In_ int32_t note) noexcept;

    // Whether a key is written with flats. F major and G minor are; G major and E minor are not.
    bool KeyUsesFlats(_In_ int32_t root, _In_ MusicalScale scale) noexcept;

    // A note written the way this app writes notes everywhere else: 48 is C3.
    std::wstring PadNoteName(_In_ int32_t note, _In_ bool flats);

    // The two intervals behind the hexagon layouts somebody might know by name: the step to the
    // next pad on the right, and the step up and to the right.
    struct HexLayoutPreset
    {
        int32_t RightInterval{ 0 };
        int32_t RowInterval{ 0 };
    };

    // Whole tones along a row, a fifth up to the right and a fourth up to the left. The
    // concertina layout, and the one most hexagon apps start from.
    constexpr HexLayoutPreset WickiHaydenLayout{ 2, 7 };

    // Major thirds along a row, a fifth up to the right and a minor third up to the left, so a
    // major or a minor chord is three pads that touch.
    constexpr HexLayoutPreset HarmonicTableLayout{ 4, 7 };

    // Whole tones along a row, and a semitone up to the right. Every other row repeats the one
    // two below it, which is what lets one fingering be played from either row.
    constexpr HexLayoutPreset JankoLayout{ 2, 1 };

    // The orders the inspector offers these in, shared by the panel that shows them and the edit
    // that reads them back, so an index in a list can never mean two things.
    constexpr MusicalScale PadScaleOrder[]
    {
        MusicalScale::Major,
        MusicalScale::Minor,
        MusicalScale::HarmonicMinor,
        MusicalScale::MelodicMinor,
        MusicalScale::Dorian,
        MusicalScale::Phrygian,
        MusicalScale::Lydian,
        MusicalScale::Mixolydian,
        MusicalScale::Locrian,
        MusicalScale::MajorPentatonic,
        MusicalScale::MinorPentatonic,
        MusicalScale::Blues,
        MusicalScale::WholeTone,
    };

    constexpr PadNoteNames PadNoteNamesOrder[]
    {
        PadNoteNames::Center,
        PadNoteNames::Top,
        PadNoteNames::Bottom,
        PadNoteNames::TopLeft,
        PadNoteNames::TopRight,
        PadNoteNames::BottomLeft,
        PadNoteNames::BottomRight,
        PadNoteNames::Hidden,
    };

    constexpr PadGlide PadGlideOrder[]
    {
        PadGlide::Off,
        PadGlide::Portamento,
        PadGlide::PerNoteBend,
    };

    // The named hexagon layouts, in the order they are offered. Anything else is a layout of
    // the customer's own.
    constexpr HexLayoutPreset HexLayoutOrder[]
    {
        WickiHaydenLayout,
        HarmonicTableLayout,
        JankoLayout,
    };

    // Where each row of square pads starts: the row below's end, then one to twelve semitones up.
    constexpr int32_t PadRowChoiceCount = 13;
}
