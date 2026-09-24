// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and composition. What color a control ends up is a property of
// the theme and the control, not of the graphics stack, and getting it wrong is the kind of thing
// that only shows up on one theme out of nine. So it is arithmetic, and it is tested.

#include <sal.h>
#include <cstdint>

#include "LayoutModel.h"
#include "ThemeModel.h"

namespace glass
{
    // Every color one control is painted with, already resolved from the theme so the renderer
    // never has to consult it again.
    struct ControlColors
    {
        // Behind everything. Transparent where the theme wants the deck to show through
        // untouched, which is what High contrast asks for.
        ThemeColor Plate{};

        // The hairline that is the control's entire resting identity.
        ThemeColor Rim{};

        // The value, as a bar or an arc.
        ThemeColor Pipe{};

        // The part of the travel the value has not reached.
        ThemeColor Track{};

        // Lifts on touch and on incoming activity, and decays. Activity is the only thing that
        // blooms.
        ThemeColor Bloom{};

        ThemeColor Label{};
    };

    // The control's own hue. A slot unless the control asked for a literal color and gave one
    // that parses; a color that does not parse falls back to the slot rather than to black,
    // because black on a black deck is an invisible control.
    ThemeColor ResolveHue(_In_ Control const& control, _In_ Theme const& theme) noexcept;

    ControlColors ResolveControlColors(_In_ Control const& control, _In_ Theme const& theme) noexcept;

    // Straight source-over, with the amount scaling the top color's own alpha.
    ThemeColor BlendOver(
        _In_ ThemeColor const& base,
        _In_ ThemeColor const& over,
        _In_ double amount) noexcept;

    // A label has to be readable on whatever the deck turned out to be, so it is chosen by
    // measuring rather than by assuming the theme is dark.
    ThemeColor ReadableInk(_In_ ThemeColor const& background) noexcept;

    // Lamps stop separating below the theme's own floor and the ring reads as a fine comb, so a
    // knob that small falls back to a solid arc on its own.
    bool UsesLampRing(
        _In_ Theme const& theme,
        _In_ double controlWidth,
        _In_ double controlHeight) noexcept;
}
