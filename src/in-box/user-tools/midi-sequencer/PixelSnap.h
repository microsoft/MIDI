// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Lines that move, such as the playhead, drawn on whole physical pixels. A 1.5-DIP line at a
// fractional position is smoothed across two or three pixels and changes width as it moves, so it
// seems to fade in and out (#1324, at 150% on a 4K screen).

#include <algorithm>
#include <cmath>

namespace midisequencer
{
    // A position in DIPs moved to the nearest physical pixel. scale is physical pixels per DIP.
    inline float SnapToPixel(float dips, float scale) noexcept
    {
        return scale > 0.0f ? std::round(dips * scale) / scale : dips;
    }

    inline double SnapToPixel(double dips, double scale) noexcept
    {
        return scale > 0.0 ? std::round(dips * scale) / scale : dips;
    }

    // A width in DIPs as a whole number of physical pixels, at least one.
    inline float WholePixels(float dips, float scale) noexcept
    {
        return scale > 0.0f ? std::max(1.0f, std::round(dips * scale)) / scale : dips;
    }

    inline double WholePixels(double dips, double scale) noexcept
    {
        return scale > 0.0 ? std::max(1.0, std::round(dips * scale)) / scale : dips;
    }
}
