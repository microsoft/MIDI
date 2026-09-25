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

        // How strong a control's rim is when nothing is happening to it.
        constexpr double RestingRimAlpha = 0.28;

        // Tick marks and center dots are orientation, not information. They have to be findable
        // when looked for and invisible when not.
        constexpr uint8_t MarkAlpha = 46;
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
            // Glass: the theme's own smoked tint at whatever opacity it asked for, laid over
            // whatever the deck is. A tint of zero means the deck shows through untouched,
            // which High contrast asks for.
            colors.Plate = theme.GlassColor;
            colors.Plate.A = static_cast<uint8_t>(
                std::clamp(std::lround(255.0 * theme.GlassTintPercent / 100.0), 0L, 255L));
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
            // A quarter strength, not the full hue. Nothing is saturated at rest - the rim is
            // there to say which control this is, and the value and the activity are the only
            // things allowed to be bright. A full-strength rim on every control turns a busy
            // page into a grid of neon rectangles and nothing stands out at all.
            colors.Rim = hue;
            colors.Rim.A = static_cast<uint8_t>(std::clamp(std::lround(hue.A * RestingRimAlpha), 0L, 255L));
            break;
        }

        colors.Bloom = hue;
        colors.Bloom.A = static_cast<uint8_t>(
            std::clamp(std::lround(255.0 * theme.GlowStrength / 100.0), 0L, 255L));

        // The bar is brightest where the value is and falls away behind it. A falloff of one
        // means the theme wants a flat bar, which is what the tonal themes and Bigwig ask for.
        colors.PipeEnd = hue;
        colors.PipeEnd.A = static_cast<uint8_t>(
            std::clamp(std::lround(hue.A * theme.PipeFalloff), 0L, 255L));

        colors.Sheen = { 255, 255, 255, static_cast<uint8_t>(
            std::clamp(std::lround(255.0 * theme.PlateSheenPercent / 100.0), 0L, 255L)) };

        switch (theme.Thumb)
        {
        case ThumbStyle::Hue:
            colors.Thumb = hue;
            colors.ThumbEnd = hue;
            colors.ThumbLine = { hue.R, hue.G, hue.B, 0 };
            break;

        case ThumbStyle::Neutral:
            colors.Thumb = theme.ThumbColor;
            colors.ThumbEnd = theme.ThumbEndColor;
            colors.ThumbLine = hue;
            break;

        case ThumbStyle::None:
        default:
            colors.Thumb = { 0, 0, 0, 0 };
            colors.ThumbEnd = { 0, 0, 0, 0 };
            colors.ThumbLine = { 0, 0, 0, 0 };
            break;
        }

        // On is the plate itself carrying the hue, top brighter than bottom. The same two
        // numbers on every theme: what changes between them is the hue and whether it glows.
        colors.OnPlate = hue;
        colors.OnPlate.A = static_cast<uint8_t>(std::clamp(std::lround(hue.A * 0.34), 0L, 255L));

        colors.OnPlateEnd = hue;
        colors.OnPlateEnd.A = static_cast<uint8_t>(std::clamp(std::lround(hue.A * 0.17), 0L, 255L));

        colors.OnRim = hue;
        colors.OnRim.A = static_cast<uint8_t>(std::clamp(std::lround(hue.A * 0.90), 0L, 255L));

        // The label sits on the plate where there is one, and on the deck where there is not.
        colors.Label = ReadableInk(
            colors.Plate.A >= 128
            ? BlendOver(theme.Deck.Color, colors.Plate, 1.0)
            : theme.Deck.Color);

        colors.Pointer = theme.Rim == RimSource::NeutralEdge ? colors.Label : hue;

        colors.Marks = colors.Label;
        colors.Marks.A = MarkAlpha;

        return colors;
    }

    _Use_decl_annotations_
    bool UsesLampRing(Theme const& theme, double controlWidth, double controlHeight) noexcept
    {
        return theme.ValueIndicator == ValueIndicatorStyle::SegmentedLamps &&
            std::min(controlWidth, controlHeight) >= theme.MinimumLampRingSize;
    }
}
