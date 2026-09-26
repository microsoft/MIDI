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
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // What a palette tile draws above its name.
    //
    // A font glyph cannot tell a fader from a fader bank, and in the design those two are one
    // tile apart. So a tile draws a miniature of the control instead, described here as a few
    // numbers rather than as a shape per kind.
    enum class PaletteArtShape
    {
        // A rounded rectangle, outlined, filled, or both. Covers button, toggle, pad, wheel,
        // range, XY pad and ribbon.
        Rectangle = 0,

        // A circle. Knob, encoder, joystick, lamp.
        Ellipse = 1,

        // Upright bars, for a bank of faders.
        VerticalBars = 2,

        // Bars lying down, for a step sequence.
        HorizontalBars = 3,

        // A Segoe Fluent Icons code point, for the few that have no useful miniature.
        Glyph = 4,

        // A short monospace sample: "0.00" for a readout, "1" for a stepper.
        Sample = 5,

        // A bar filled from the bottom up, for a meter.
        MeterBar = 6,

        // A sine, for an LFO.
        Wave = 7,

        // White keys with a few black ones on top, for the piano keyboard.
        Keys = 8,
    };

    struct PaletteArt
    {
        PaletteArtShape Shape{ PaletteArtShape::Rectangle };

        double Width{ 22 };
        double Height{ 14 };
        double CornerRadius{ 3 };

        // Of the accent color. Zero means the tile does not draw that part at all.
        double StrokeOpacity{ 0.8 };
        double FillOpacity{ 0.0 };

        std::wstring Sample{};
    };

    // What the palette offers, in the order and grouping the design puts them in. The name is a
    // resource key rather than text, because this layer never looks at a resource file.
    struct PaletteEntry
    {
        ControlKind Kind{ ControlKind::Knob };
        std::wstring NameResourceKey{};
        std::wstring GroupResourceKey{};

        // A Segoe Fluent Icons code point. The outline uses it; a tile draws its art instead.
        wchar_t Glyph{ 0 };

        PaletteArt Art{};

        // A kind the design calls for that the document model does not carry yet. It is in the
        // palette so the shape of the palette is the shape the design agreed, and so nobody
        // hunts for a control that was never built. It cannot be dropped on a page.
        bool IsComing{ false };
    };

    std::vector<PaletteEntry> const& Palette() noexcept;

    // The group headings, in order, with no duplicates. The palette is drawn group by group.
    std::vector<std::wstring> PaletteGroups();

    // A control dropped on the page, ready to use rather than ready to configure.
    //
    // A new control that sends nothing is a control somebody has to visit the MIDI tab for
    // before it does anything at all, so a fader arrives bound to a control change and a pad
    // arrives bound to a note. The numbers walk up from the ones already on the page, which is
    // what stops eight dropped faders all being controller 1.
    Control MakeNewControl(
        _In_ ControlKind kind,
        _In_ double x,
        _In_ double y,
        _In_ int32_t pageWidth,
        _In_ int32_t pageHeight,
        _In_ std::wstring const& deviceName,
        _In_ Page const& page);

    // Which controller or note number a new control of this kind should take on a page that
    // already holds these controls: the lowest one nothing is using.
    uint32_t NextFreeNumber(_In_ Page const& page, _In_ MessageKind kind, _In_ uint32_t first) noexcept;

    // Whether this kind of control sends anything at all. A meter, a lamp, a readout, a label
    // and an image do not; they display.
    bool SendsAnything(_In_ ControlKind kind) noexcept;

    // Whether this kind is square by nature, so it arrives with its aspect locked. A knob
    // stretched into an ellipse is nobody's idea of a knob.
    bool IsSquareByNature(_In_ ControlKind kind) noexcept;
}
