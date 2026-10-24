// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What grid buttons, pickers and readouts do to a fretboard while the layout runs: the capo,
// which frets show, the tuning, and the key its dots show. Deliberately free of pch.h and XAML,
// like the rest of the document layer. Nothing here is saved.

#include <sal.h>
#include <cstdint>
#include <string>

#include "Fretboard.h"
#include "PadGridChanges.h"

namespace glass
{
    // A fretboard as it plays with these changes laid over it.
    FretboardSpec WithFretState(_In_ FretboardSpec spec, _In_ PadGridState const& state);

    // Whether a pad grid row function does anything to a fretboard. The capo, the neck, muting,
    // the key's root and scale, and reset do; octaves, rows and in key only are for pads.
    bool ReachesFretboards(_In_ PadGridFunction function) noexcept;

    // One change made to one fretboard. A change it can't make, like a capo past the last fret,
    // leaves it as it was.
    PadGridState ChangedFretState(
        _In_ FretboardSpec const& built,
        _In_ PadGridState state,
        _In_ PadGridFunction function);

    // A fretboard's frets showing, moved to start at this fret. Where a finger dragging the neck
    // strip puts them.
    PadGridState MovedNeckState(
        _In_ FretboardSpec const& built,
        _In_ PadGridState state,
        _In_ int32_t firstFret);

    // The tuning a tuning picker's position names.
    FretTuning TuningAtPosition(_In_ int32_t position) noexcept;

    // The first fretboard a picker or a readout targets, or null. With needsKey, the first one
    // that shows a key.
    Control const* ShownFretboard(
        _In_ LayoutDocument const& document,
        _In_ PadGridTargets const& targets,
        _In_ bool needsKey) noexcept;

    // What a readout with a fretboard source reads.
    struct FretboardReading
    {
        bool HasFretboard{ false };
        int32_t Capo{ 0 };
        FretTuning Tuning{ FretTuning::Standard };
    };

    FretboardReading ReadFretboard(
        _In_ LayoutDocument const& document,
        _In_ PadGridTargets const& targets,
        _In_ PadGridStates const& states) noexcept;
}
