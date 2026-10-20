// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Sizes that are a whole number of DIPs and of pixels at a display scale, for Win2D canvases
// (ThemeCanvas.h). No dependencies, so the tests can take it.

#include <cmath>

namespace themeengine
{
    // The fewest DIPs that make a whole number of pixels at the scale: 1 at 100% and 200%, 2 at
    // 150%, 4 at 125% and 175%. 1 for a scale with none up to 8.
    inline double WholePixelStep(double scale) noexcept
    {
        if (!std::isfinite(scale) || scale <= 0.0)
        {
            return 1.0;
        }

        for (int step = 1; step <= 8; ++step)
        {
            auto const pixels = static_cast<double>(step) * scale;

            if (std::abs(pixels - std::round(pixels)) < 1e-6)
            {
                return static_cast<double>(step);
            }
        }

        return 1.0;
    }

    // The longest length, no longer than what's available, that's a whole number of steps. What
    // layout gives is a whole number of pixels written in DIPs, which can come out a hair short
    // (1023 pixels at 175% is 584.5714 DIPs), so a hair is forgiven.
    inline double WholePixelLength(double available, double scale) noexcept
    {
        if (!std::isfinite(available) || available <= 0.0)
        {
            return 0.0;
        }

        auto const step = WholePixelStep(scale);
        return std::floor(available / step + 1e-6) * step;
    }
}
