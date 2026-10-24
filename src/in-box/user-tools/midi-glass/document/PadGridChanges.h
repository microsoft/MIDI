// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Grid buttons, pickers and readouts: what changes a pad grid while the layout runs, and on which
// grids. Deliberately free of pch.h and XAML, like the rest of the document layer, so the rules
// for an octave shift or the next scale are tested without a window.
//
// Nothing here is saved. A layout opens as it was built each time, and Try mode forgets every
// change when it ends.

#include <sal.h>
#include <cstdint>
#include <map>
#include <string>

#include "LayoutModel.h"
#include "PadGrid.h"

namespace glass
{
    // What has been changed on one grid since the layout opened. Every field at its default is
    // the grid as it was built.
    struct PadGridState
    {
        // A root note, 0 to 11, or -1 for the grid's own.
        int32_t Root{ -1 };

        // A MusicalScale, or -1 for the grid's own.
        int32_t Scale{ -1 };

        // -1 for the grid's own, 0 for every note, 1 for in key only.
        int32_t InKeyOnly{ -1 };

        // An index into the row offsets a picker offers, or -1 for the grid's own.
        int32_t RowOffset{ -1 };

        // Octaves, rows and steps the whole grid has moved.
        int32_t Octaves{ 0 };
        int32_t Rows{ 0 };
        int32_t Steps{ 0 };

        bool operator==(PadGridState const&) const = default;
    };

    // By control id. A grid with nothing changed has no entry.
    using PadGridStates = std::map<std::wstring, PadGridState>;

    PadGridState StateOf(_In_ PadGridStates const& states, _In_ std::wstring const& id);

    // The row offsets a row offset picker offers, and the order Next row offset steps through:
    // continuous, a third, a fourth, a fifth and an octave. Semitones for every note, and notes
    // of the scale in key, so a fourth is a fourth either way.
    constexpr int32_t RowOffsetSemitones[RowOffsetPickerPositions]{ 0, 4, 5, 7, 12 };
    constexpr int32_t RowOffsetScaleSteps[RowOffsetPickerPositions]{ 0, 2, 3, 4, 7 };

    // How far an octave picker reaches either way.
    constexpr int32_t OctavePickerReach = 2;

    // A grid as it plays with these changes laid over it. A root, a scale or in key only only
    // reach a grid that has a key.
    PadGridSpec WithGridState(_In_ PadGridSpec spec, _In_ bool hex, _In_ PadGridState const& state) noexcept;

    // Whether a row, a picker or a readout works on this grid.
    bool TargetsGrid(_In_ PadGridTargets const& targets, _In_ std::wstring const& id) noexcept;

    // One change made to one grid. A change that would push the bottom left pad off either end
    // of the note range is not made, so the grid can always come back.
    PadGridState ChangedState(
        _In_ PadGridSpec const& built,
        _In_ bool hex,
        _In_ PadGridState state,
        _In_ PadGridFunction function) noexcept;

    // A pad grid row pressed: the change made to every grid it targets. False when nothing
    // changed.
    bool ApplyPadGridFunction(
        _In_ LayoutDocument const& document,
        _In_ PadGridTargets const& targets,
        _In_ PadGridFunction function,
        _Inout_ PadGridStates& states);

    // A picker moved to a position. False when nothing changed, or the control is not a picker.
    bool ApplyPick(
        _In_ LayoutDocument const& document,
        _In_ Control const& picker,
        _In_ int32_t position,
        _Inout_ PadGridStates& states);

    // The grid a picker or a readout shows: the first one it targets, and for a root or scale
    // picker the first of those with a key. Null when there is none.
    Control const* ShownGrid(
        _In_ LayoutDocument const& document,
        _In_ PadGridTargets const& targets,
        _In_ bool needsKey) noexcept;

    // The position a picker lights for the grid it shows, or -1 for none.
    int32_t PickerPositionFor(
        _In_ LayoutDocument const& document,
        _In_ Control const& picker,
        _In_ PadGridStates const& states) noexcept;

    // The same as a switch value, 0 to 1. Negative when it lights nothing.
    double PickerValueFor(
        _In_ LayoutDocument const& document,
        _In_ Control const& picker,
        _In_ PadGridStates const& states) noexcept;

    // Whether a picker covers this grid: it picks this, and targets it.
    bool HasPickerFor(
        _In_ LayoutDocument const& document,
        _In_ SwitchPicks picks,
        _In_ std::wstring const& gridId) noexcept;

    // What a readout with a pad grid source reads from the grid it shows.
    struct PadGridReading
    {
        bool HasGrid{ false };
        bool HasKey{ false };
        int32_t Root{ 0 };
        MusicalScale Scale{ MusicalScale::Major };
        bool InKeyOnly{ false };
        int32_t Octaves{ 0 };

        // The row offset in the grid's own units: semitones, or notes of the scale in key.
        // Zero is continuous.
        int32_t RowOffset{ 0 };
    };

    PadGridReading ReadPadGrid(
        _In_ LayoutDocument const& document,
        _In_ PadGridTargets const& targets,
        _In_ PadGridStates const& states) noexcept;

    // Whether a readout shows a pad grid rather than its own value.
    bool ReadsAPadGrid(_In_ Control const& control) noexcept;

    // An octave the way a readout prints it: "0", "+1", "-2" with a real minus sign.
    std::wstring SignedNumberText(_In_ int32_t value);
}
