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

        // The bottom of the plate, where the theme lifts its top. The same color again on a
        // theme with a flat plate, which is most of them.
        ThemeColor PlateEnd{};

        // What the plate becomes under a finger. Transparent where the theme has a glow to lift
        // instead, which is every theme built out of glass.
        //
        // The alpha is written out because ThemeColor defaults ITS alpha to 255, so a plain
        // ThemeColor member is opaque BLACK rather than nothing. That default turned "this theme
        // has no touch fill" into a black disc over every control the moment it was pressed.
        ThemeColor TouchPlate{ 0, 0, 0, 0 };
        ThemeColor TouchPlateEnd{ 0, 0, 0, 0 };

        // The rim under a finger. The design's own rule: the white halo alone is a weak signal,
        // so the rim comes up with it every time and touch is never carried by a glow alone.
        ThemeColor TouchRim{ 0, 0, 0, 0 };

        // The hairline that is the control's entire resting identity.
        ThemeColor Rim{};

        // The value, as a bar or an arc.
        ThemeColor Pipe{};

        // The far end of that bar. The same hue at a lower alpha on the glass themes, the same
        // color again on the flat ones.
        ThemeColor PipeEnd{};

        // The part of the travel the value has not reached.
        ThemeColor Track{};

        // The same, for a knob's arc. Not the same question: a slot is a recess cut into the
        // plate and an arc sits outside it on bare deck, so on a dark theme one is a dark and
        // the other has to be a faint light.
        ThemeColor ArcTrack{};

        // The two ends of the cap on a fader, already resolved from the theme's thumb style.
        // Both transparent when the theme draws no cap.
        ThemeColor Thumb{};
        ThemeColor ThumbEnd{};

        // The hairline of hue through a neutral cap. Transparent when the cap is the hue
        // itself, because a hue line on a hue cap is invisible.
        ThemeColor ThumbLine{};

        // The sheen down the top of the plate. Alpha 0 on a flat theme.
        ThemeColor Sheen{};

        // Lifts on touch and on incoming activity, and decays. Activity is the only thing that
        // blooms.
        ThemeColor Bloom{};

        // How much of that light is spilled when nothing is happening, 0 to 1. Zero on every
        // theme where the plate can separate itself from the deck by value alone.
        double RestingGlow{ 0.0 };

        // The line on a knob that says which way it is pointing. Usually the hue; on a theme
        // with a neutral edge it is the deck's own ink, because on those the hue is reserved
        // for the value and a pointer is not the value.
        ThemeColor Pointer{};

        // Tick marks beside a fader, and the center dot on a knob. Barely there on purpose.
        ThemeColor Marks{};

        ThemeColor Label{};

        // A meter's three zones, already resolved from the theme's own choice of slots.
        ThemeColor MeterLit{};
        ThemeColor MeterWarn{};
        ThemeColor MeterHot{};

        // What the plate becomes while the control is on, and the rim that goes with it.
        ThemeColor OnPlate{};
        ThemeColor OnPlateEnd{};
        ThemeColor OnRim{};

        // The name of a switch that carries it inside itself, at rest and lit. Measured against
        // the plate it is sitting on in each state, because a lit plate can be the hue outright
        // and a light name on a yellow tab is not a name at all.
        ThemeColor SwitchInk{};
        ThemeColor SwitchInkOn{};

        // The lamp a switch lights instead of filling, and the line around the switch while it
        // is lit. Both transparent on a theme that fills.
        ThemeColor Lamp{ 0, 0, 0, 0 };
        ThemeColor LampRim{ 0, 0, 0, 0 };

        // A knob's top face, and the smaller cap on it. The face is the plate again on a theme
        // that names neither; the cap is transparent on a theme that has none.
        ThemeColor KnobFace{};
        ThemeColor KnobFaceEnd{};
        ThemeColor KnobCap{ 0, 0, 0, 0 };
        ThemeColor KnobCapEnd{ 0, 0, 0, 0 };

        // The printed ring of marks around a knob, where the theme draws one.
        ThemeColor KnobTick{ 0, 0, 0, 0 };

        // A fader's fill, already at the theme's own strength. The same as the pipe on every
        // theme that fills a fader outright.
        ThemeColor Fill{};
        ThemeColor FillEnd{};

        // The field a display is sunk into, the shade inside anything cut into the surface,
        // the shade up from the bottom of a plate and the light line along its top. All four
        // transparent on a theme that asks for none of them.
        ThemeColor Well{ 0, 0, 0, 0 };
        ThemeColor Recess{ 0, 0, 0, 0 };
        ThemeColor PlateShade{ 0, 0, 0, 0 };
        ThemeColor PlateHighlight{ 0, 0, 0, 0 };

        // The line around a grouping panel.
        ThemeColor PanelOutline{ 0, 0, 0, 0 };

        // A piano keyboard's natural keys and its sharps and flats.
        ThemeColor KeyWhite{};
        ThemeColor KeyBlack{};
    };

    // The control's own hue. A slot unless the control asked for a literal color and gave one
    // that parses; a color that does not parse falls back to the slot rather than to black,
    // because black on a black deck is an invisible control.
    ThemeColor ResolveHue(_In_ Control const& control, _In_ Theme const& theme) noexcept;

    // A control that is on or off rather than somewhere along a travel. Its plate is what
    // carries the state, which is the one place a hue is allowed to fill an area - and it is
    // also the only kind a theme can fill at rest while leaving its knobs bare.
    bool IsSwitchControl(_In_ ControlKind kind) noexcept;

    // A switch whose plate carries a fill at rest and when it is on. Everything IsSwitchControl
    // says apart from an LFO: its plate is where the wave is drawn, and a wave the same color as
    // the plate under it cannot be seen at all.
    bool FillsLikeASwitch(_In_ ControlKind kind) noexcept;

    // The theme's own ink where it is readable on this background, and a measured one where it
    // is not. Type needs 4.5 : 1, and a named ink is a preference rather than a promise.
    ThemeColor InkOn(_In_ ThemeColor const& themeInk, _In_ ThemeColor const& background) noexcept;

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
