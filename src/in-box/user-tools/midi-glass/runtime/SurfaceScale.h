// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. Where a page sits inside a window is arithmetic, and a
// mistake in it puts every control a few pixels from where the finger lands, so it is tested as
// arithmetic.

#include <sal.h>
#include <cstdint>

#include "LayoutModel.h"

namespace glass
{
    // A page has a fixed pixel size and is never stretched, so the only questions are how much it
    // is scaled by and where the top left corner of it sits.
    struct SurfaceViewport
    {
        double Scale{ 1.0 };

        // Where the page's top left corner sits inside the window, in window pixels. Negative
        // never happens: a page larger than the window starts at the origin and scrolls.
        double OffsetX{ 0.0 };
        double OffsetY{ 0.0 };

        double ContentWidth{ 0.0 };
        double ContentHeight{ 0.0 };

        // The page does not fit, so the window needs scroll bars. Only ever true for actual size
        // and for a custom percentage; fitting cannot overflow.
        bool NeedsScrolling{ false };
    };

    // Actual size is the default, because a performer's muscle memory is worth more than filling
    // the window. Fit scales both axes by the same number and letterboxes what is left.
    constexpr double MinimumCustomScalePercent = 10.0;
    constexpr double MaximumCustomScalePercent = 400.0;

    SurfaceViewport ComputeViewport(
        _In_ int32_t pageWidth,
        _In_ int32_t pageHeight,
        _In_ double viewportWidth,
        _In_ double viewportHeight,
        _In_ ScaleMode mode,
        _In_ double customScalePercent) noexcept;

    // A point in the window to a point on the page. False when the point is outside the page,
    // which is the letterbox bar and must not be treated as the nearest control.
    bool ViewportToPage(
        _In_ SurfaceViewport const& viewport,
        _In_ int32_t pageWidth,
        _In_ int32_t pageHeight,
        _In_ double viewportX,
        _In_ double viewportY,
        _Out_ double& pageX,
        _Out_ double& pageY) noexcept;
}
