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
}
