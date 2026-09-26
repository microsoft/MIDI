// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and Win2D. Working out what a thumbnail contains is arithmetic
// and belongs in a test; only putting pixels on a surface needs a graphics device.

#include <sal.h>
#include <cstdint>
#include <vector>

#include "LayoutModel.h"
#include "ThemeModel.h"

namespace glass
{
    struct ThumbnailRect
    {
        double X{ 0 };
        double Y{ 0 };
        double Width{ 0 };
        double Height{ 0 };
    };

    struct ThumbnailItem
    {
        ThumbnailRect Bounds{};
        ControlKind Kind{ ControlKind::Knob };

        // Already resolved from the control's slot, so the renderer never needs the theme.
        ThemeColor Hue{};
    };

    // Everything needed to draw a card, with no layout model and no theme left to consult.
    struct ThumbnailPlan
    {
        int32_t Width{ 0 };
        int32_t Height{ 0 };

        // Where the page sits inside the image. A page is a fixed size and scales letterboxed,
        // never stretched, so a card shows the same proportions the layout really has.
        ThumbnailRect PageBounds{};

        ThemeColor DeckColor{};

        // The deck is lit from just above its top edge rather than filled flat, which is what
        // makes a surface read as glass instead of paper. Both ends are derived from the deck,
        // so a light theme stays light.
        ThemeColor DeckTopColor{};
        ThemeColor DeckBottomColor{};

        // The bars either side of a letterboxed page. Darker than the deck so the page reads as
        // the object and the bar reads as nothing.
        ThemeColor SurroundColor{};

        double CornerRadius{ 0 };
        double PlateOpacity{ 0.86 };

        std::vector<ThumbnailItem> Items{};
    };

    // A thumbnail is drawn from the layout model rather than captured from a window, so it is
    // correct before the layout has ever been opened on this PC and costs nothing to produce.
    //
    // Controls that fall outside the page are left out. They are real and the editor still shows
    // them, but they are not part of what ships, and a card that drew them would misrepresent the
    // layout at exactly the moment somebody is choosing between layouts.
    ThumbnailPlan PlanThumbnail(
        _In_ LayoutDocument const& document,
        _In_ Theme const& theme,
        _In_ int32_t imageWidth,
        _In_ int32_t imageHeight,
        _In_ size_t pageIndex = 0) noexcept;

    // The sizes the library asks for. A favorite is a large card, a recent one is small.
    constexpr int32_t LargeThumbnailWidth = 480;
    constexpr int32_t LargeThumbnailHeight = 300;
    constexpr int32_t SmallThumbnailWidth = 240;
    constexpr int32_t SmallThumbnailHeight = 150;
}
