// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SurfaceColors.h"
#include "ThemeStore.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        uint8_t Mix(_In_ uint8_t base, _In_ uint8_t over, _In_ double amount) noexcept
        {
            return static_cast<uint8_t>(std::clamp(
                std::lround(base + (over - base) * amount), 0L, 255L));
        }
    }

    _Use_decl_annotations_
    ThemeColor BlendOver(ThemeColor const& base, ThemeColor const& over, double amount) noexcept
    {
        auto const weight = std::clamp(amount, 0.0, 1.0) * (over.A / 255.0);

        return
        {
            Mix(base.R, over.R, weight),
            Mix(base.G, over.G, weight),
            Mix(base.B, over.B, weight),
            base.A
        };
    }

    _Use_decl_annotations_
    ThemeColor ReadableInk(ThemeColor const& background) noexcept
    {
        // The two candidates a surface actually wants, measured against the background rather
        // than picked from the theme's name. A theme with a mid grey deck gets whichever wins.
        constexpr ThemeColor light{ 0xF2, 0xF3, 0xF5, 255 };
        constexpr ThemeColor dark{ 0x10, 0x11, 0x14, 255 };

        return ContrastRatio(light, background) >= ContrastRatio(dark, background) ? light : dark;
    }

    _Use_decl_annotations_
    ThemeColor ResolveHue(Control const& control, Theme const& theme) noexcept
    {
        if (control.HueSlot >= 0 && control.HueSlot < ThemeHueSlotCount)
        {
            return theme.HueSlots[static_cast<size_t>(control.HueSlot)];
        }

        if (control.HueSlot == LiteralHue && !control.LiteralColor.empty())
        {
            ThemeColor literal{};

            if (TryParseColor(control.LiteralColor, literal))
            {
                return literal;
            }
        }

        return theme.HueSlots[0];
    }

    _Use_decl_annotations_
    ControlColors ResolveControlColors(Control const& control, Theme const& theme) noexcept
    {
        ControlColors colors{};

        auto const hue = ResolveHue(control, theme);

        colors.Pipe = hue;
        colors.Track = theme.TrackColor;

        if (theme.PlateColor.A != 0)
        {
            // The theme named a plate outright, which is what a raised neutral surface needs.
            colors.Plate = theme.PlateColor;
        }
        else if (theme.FillAtRest > 0.0)
        {
            // Tonal: the plate is the deck tinted with the control's own hue. That is why the
            // tonal themes cost no extra rendering layer - a tint is a background color.
            colors.Plate = BlendOver(theme.Deck.Color, hue, theme.FillAtRest);
            colors.Plate.A = 255;
        }
        else
        {
            // Glass: near black at the theme's tint, laid over whatever the deck is. A tint of
            // zero means the deck shows through untouched, which High contrast asks for.
            colors.Plate = { 0, 0, 0, static_cast<uint8_t>(
                std::clamp(std::lround(255.0 * theme.GlassTintPercent / 100.0), 0L, 255L)) };
        }

        switch (theme.Rim)
        {
        case RimSource::NeutralEdge:
            colors.Rim = theme.NeutralRimColor;
            break;

        case RimSource::None:
            colors.Rim = { hue.R, hue.G, hue.B, 0 };
            break;

        case RimSource::ControlHue:
        default:
            colors.Rim = hue;
            break;
        }

        colors.Bloom = hue;
        colors.Bloom.A = static_cast<uint8_t>(
            std::clamp(std::lround(255.0 * theme.GlowStrength / 100.0), 0L, 255L));

        // The label sits on the plate where there is one, and on the deck where there is not.
        colors.Label = ReadableInk(
            colors.Plate.A >= 128
            ? BlendOver(theme.Deck.Color, colors.Plate, 1.0)
            : theme.Deck.Color);

        return colors;
    }

    _Use_decl_annotations_
    bool UsesLampRing(Theme const& theme, double controlWidth, double controlHeight) noexcept
    {
        return theme.ValueIndicator == ValueIndicatorStyle::SegmentedLamps &&
            std::min(controlWidth, controlHeight) >= theme.MinimumLampRingSize;
    }
}
