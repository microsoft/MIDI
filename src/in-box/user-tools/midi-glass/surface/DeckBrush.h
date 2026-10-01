// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Needs XAML, so it is not part of the pure surface layer and is not in the unit tests. What it
// wraps - which colors a deck is made of - is in ThemeModel, which is.

#include "ThemeModel.h"

namespace glass
{
    // The brush for one theme's deck.
    //
    // A deck is not one color. The design's decks are lit from above: a radial fall-off from a
    // lighter top to a darker floor, which is what stops a large dark surface reading as a hole.
    // A theme that asks for a flat color still gets one.
    media::Brush MakeDeckBrush(_In_ ThemeDeck const& deck);

    // Which half of the overlay to lay down.
    //
    // A panel's GRAIN is part of the panel, so a control sitting on it covers it. A tube's
    // raster is not: the glass and the picture are the same surface, and a scan line that
    // stopped at the edge of a control would read as a mistake. The two therefore go on
    // different sides of the controls, which means two elements and two calls. There is no
    // everything-at-once: a wall picture laid in one call ends up over the controls.
    enum class DeckOverlayLayer
    {
        // The panel's own texture, its wall picture and its rain.
        BeneathControls = 1,

        // The corner fall-off, the raster and the room reflected in the glass.
        AboveControls = 2,
    };

    // The scan lines, the corner fall-off and the faceplate reflection, laid over an element as
    // ONE composition visual for the whole page. Eighty controls cost what four cost.
    //
    // `scale` is how many screen pixels one of the element's own units is. A three pixel pitch
    // measured in page units at 87 percent zoom is a beat pattern across the screen rather than
    // a row of scan lines, so the pitch is divided back out by it.
    //
    // `pageScale` is how many of the element's own units one page unit is. A deck's repeating
    // picture and its rain are part of the page, so they grow and shrink with it. `animate` lets
    // the rain fall; the designer holds it still, the way it holds a stopwatch at zero.
    //
    // A theme with no overlay clears whatever was there, so switching away from a tube theme
    // takes its glass with it.
    void ApplyDeckOverlay(
        _In_ xaml::UIElement const& element,
        _In_ Theme const& theme,
        _In_ double width,
        _In_ double height,
        _In_ double scale,
        _In_ DeckOverlayLayer layer,
        _In_ double pageScale = 1.0,
        _In_ bool animate = false);
}
