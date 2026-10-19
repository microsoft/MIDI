// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Drawing for generator clips (comp 4): a Euclidean rhythm's rings, and small piano rolls of what
// a generator plays. Win2D, from data the window hands over, like ArrangeRenderer.

#include "SequenceModel.h"
#include "ThemePalette.h"

namespace midisequencer
{
    // The color a ring is drawn in. The first ring is the clip's own color.
    Color RingColor(_In_ Color clipColor, _In_ size_t ring) noexcept;

    // Where the rings sit in a square that fits the space, outermost first.
    struct RingGeometry
    {
        float CenterX{ 0 };
        float CenterY{ 0 };
        float Outer{ 0 };
        float Spacing{ 0 };

        float Radius(_In_ size_t ring) const noexcept { return Outer - Spacing * static_cast<float>(ring); }
    };

    RingGeometry MeasureRings(_In_ float width, _In_ float height, _In_ size_t ringCount) noexcept;

    // The ring a point is on, or SIZE_MAX when it's on none of them.
    size_t RingAtPoint(_In_ RingGeometry const& geometry, _In_ size_t ringCount, _In_ float x, _In_ float y) noexcept;

    // How far round from the top a point is, clockwise, from 0 up to 1.
    double TurnAtPoint(_In_ RingGeometry const& geometry, _In_ float x, _In_ float y) noexcept;

    // The rings, with a dot for each step and a bigger one for each pulse. While it plays, each
    // ring's current step is circled and a hand shows how far through the pass it is.
    void DrawRings(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ std::vector<EuclidRing> const& rings,
        _In_ Color clipColor,
        _In_ int64_t globalStep,
        _In_ double passTurn,
        _In_ size_t selected,
        _In_ Palette const& colors);

    struct RollNote
    {
        int64_t Tick{ 0 };
        int64_t Length{ 0 };
        uint8_t Number{ 0 };
        uint16_t Velocity{ 0 };

        // New since the pass before, drawn with an outline (comp 4).
        bool Outlined{ false };
    };

    // Notes from tick 0 to `ticks`, from low to high, with a keyboard down the left when keyWidth
    // is more than 0, and the playhead when it's 0 or more.
    void DrawRoll(
        _In_ canvas::CanvasDrawingSession const& ds,
        _In_ float width,
        _In_ float height,
        _In_ std::vector<RollNote> const& notes,
        _In_ int64_t ticks,
        _In_ int64_t ticksPerBar,
        _In_ int64_t ticksPerBeat,
        _In_ uint8_t low,
        _In_ uint8_t high,
        _In_ float keyWidth,
        _In_ Color noteColor,
        _In_ int64_t playhead,
        _In_ Palette const& colors);
}
