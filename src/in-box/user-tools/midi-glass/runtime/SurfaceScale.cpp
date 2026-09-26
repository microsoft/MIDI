// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SurfaceScale.h"

#include <algorithm>

namespace glass
{
    _Use_decl_annotations_
    SurfaceViewport ComputeViewport(
        int32_t pageWidth,
        int32_t pageHeight,
        double viewportWidth,
        double viewportHeight,
        ScaleMode mode,
        double customScalePercent) noexcept
    {
        SurfaceViewport viewport{};

        if (pageWidth <= 0 || pageHeight <= 0 || viewportWidth <= 0.0 || viewportHeight <= 0.0)
        {
            return viewport;
        }

        switch (mode)
        {
        case ScaleMode::FitToScreen:
            viewport.Scale = std::min(
                viewportWidth / pageWidth,
                viewportHeight / pageHeight);
            break;

        case ScaleMode::Custom:
            viewport.Scale = std::clamp(
                customScalePercent, MinimumCustomScalePercent, MaximumCustomScalePercent) / 100.0;
            break;

        case ScaleMode::ActualSize:
        default:
            viewport.Scale = 1.0;
            break;
        }

        viewport.ContentWidth = pageWidth * viewport.Scale;
        viewport.ContentHeight = pageHeight * viewport.Scale;

        // Centered when there is room, hard against the origin when there is not. A page that
        // started part way down its own scroll extent would hide its top row.
        viewport.OffsetX = std::max(0.0, (viewportWidth - viewport.ContentWidth) / 2.0);
        viewport.OffsetY = std::max(0.0, (viewportHeight - viewport.ContentHeight) / 2.0);

        viewport.NeedsScrolling =
            viewport.ContentWidth > viewportWidth + 0.5 ||
            viewport.ContentHeight > viewportHeight + 0.5;

        return viewport;
    }

    _Use_decl_annotations_
    bool ViewportToPage(
        SurfaceViewport const& viewport,
        int32_t pageWidth,
        int32_t pageHeight,
        double viewportX,
        double viewportY,
        double& pageX,
        double& pageY) noexcept
    {
        pageX = 0.0;
        pageY = 0.0;

        if (viewport.Scale <= 0.0)
        {
            return false;
        }

        pageX = (viewportX - viewport.OffsetX) / viewport.Scale;
        pageY = (viewportY - viewport.OffsetY) / viewport.Scale;

        return pageX >= 0.0 && pageY >= 0.0 && pageX < pageWidth && pageY < pageHeight;
    }
}
