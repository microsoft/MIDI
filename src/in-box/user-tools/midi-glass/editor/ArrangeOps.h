// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include <sal.h>
#include <cstdint>
#include <vector>

#include "EditGeometry.h"

namespace glass
{
    enum class ArrangeAxis
    {
        Horizontal = 0,
        Vertical = 1,
    };

    enum class AlignEdge
    {
        Left = 0,
        CenterX = 1,
        Right = 2,
        Top = 3,
        CenterY = 4,
        Bottom = 5,
    };

    // Lines a selection up. Returns a rectangle per input, in the order they came in, so the
    // caller can write them back against its own ids without this layer knowing what an id is.
    std::vector<EditRect> AlignRects(_In_ std::vector<EditRect> const& rects, _In_ AlignEdge edge);

    // The gaps between a selection, measured along one axis, in position order. One fewer than
    // the number of rectangles. Overlapping rectangles give a negative gap, which is true and
    // worth showing rather than hiding.
    std::vector<double> MeasureGaps(_In_ std::vector<EditRect> const& rects, _In_ ArrangeAxis axis);

    // Whether every gap is already the same, within a pixel. What decides whether the spacing
    // pills show one number or several.
    bool GapsAreEqual(_In_ std::vector<double> const& gaps) noexcept;

    // Sets every gap to the same number, keeping the first rectangle where it is. This is what
    // typing in a spacing pill does.
    std::vector<EditRect> SetGap(_In_ std::vector<EditRect> const& rects, _In_ ArrangeAxis axis, _In_ double gap);

    // Equalizes the gaps without moving the outermost two, so a bank keeps the width somebody
    // already chose. Three or more rectangles, or nothing happens.
    std::vector<EditRect> DistributeEvenly(_In_ std::vector<EditRect> const& rects, _In_ ArrangeAxis axis);

    // Which way a control moves through the stack. A page is drawn in the order its controls are
    // stored, first at the back, so this is a reorder of that list and nothing else.
    enum class ZOrderMove
    {
        ToBack = 0,
        Backward = 1,
        Forward = 2,
        ToFront = 3,
    };

    // The new order of positions after moving the selected ones, as indices into the original
    // list. A selection keeps its own relative order and moves as a block, and a block already
    // against the end does not slide past itself.
    std::vector<size_t> ReorderForZ(
        _In_ size_t count,
        _In_ std::vector<size_t> const& selected,
        _In_ ZOrderMove move);
}
