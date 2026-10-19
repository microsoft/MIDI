// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The symbol for each kind of clip (design section 27). The launcher's slots, the editor's header
// and the menus that make clips all use these, so a drum pattern, a melodic pattern and each kind
// of generator look the same everywhere. Most are characters from the Segoe Fluent Icons font.
// The notes clip, the Euclidean rhythm and most of section 28's generators are drawn, because no
// character looks like them.

#include "SequenceModel.h"
#include "ThemePalette.h"

namespace midisequencer
{
    enum class ClipIcon : uint8_t
    {
        Notes = 0,
        DrumPattern = 1,
        MelodicPattern = 2,
        Euclidean = 3,
        ChanceMelody = 4,
        Arpeggiator = 5,

        // Design section 28's generators.
        Life = 6,               // a glider
        LifeDrums = 7,          // a glider of rings, like drum heads
        Learn = 8,              // notes joined by the moves between them
        Words = 9,
        Evolve = 10,            // a twist, like a strand of DNA
        Infinity = 11,
        LSystem = 12,           // a branching stem
        ChangeRinging = 13,     // a bell
    };

    ClipIcon IconFor(_In_ Clip const& clip) noexcept;
    ClipIcon IconFor(_In_ PatternMode mode) noexcept;
    ClipIcon IconFor(_In_ GeneratorType type) noexcept;

    // The icon font's character for an icon, or nullptr for one that's drawn.
    wchar_t const* ClipIconGlyph(_In_ ClipIcon icon) noexcept;

    // The drawn icons as XAML path markup on a 16 unit square, which a PathIcon fills. Empty for
    // the icons that are characters.
    std::wstring_view ClipIconPath(_In_ ClipIcon icon) noexcept;

    // For a menu item, a button or a header: a FontIcon, or a PathIcon for a drawn icon.
    controls::IconElement MakeClipIcon(_In_ ClipIcon icon);

    // Draws one of section 28's drawn icons in a square at x, y. False for any other icon.
    bool DrawClipIcon(_In_ canvas::CanvasDrawingSession const& ds, _In_ ClipIcon icon, _In_ float x, _In_ float y, _In_ float size, _In_ Color color);
}
