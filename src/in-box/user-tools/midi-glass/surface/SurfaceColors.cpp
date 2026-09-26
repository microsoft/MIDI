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

        // How strong a control's rim is when nothing is happening to it. The theme's own
        // RimStrengthPercent, as a fraction; this is only the fallback for a half-written file.
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
        // than picked from the theme's name. A theme with a mid gray deck gets whichever wins.
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

        // The one un-hued color. A theme without one falls back to the first hue rather than to
        // nothing, because an invisible control is worse than a wrongly colored one.
        if (control.HueSlot == NeutralSlot && HasNeutralColor(theme))
        {
            return theme.NeutralColor;
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
    bool IsSwitchControl(ControlKind kind) noexcept
    {
        switch (kind)
        {
        case ControlKind::Button:
        case ControlKind::Toggle:
        case ControlKind::Pad:
        case ControlKind::PageTab:
        case ControlKind::Lamp:
        case ControlKind::Lfo:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool FillsLikeASwitch(ControlKind kind) noexcept
    {
        return IsSwitchControl(kind) && kind != ControlKind::Lfo;
    }

    _Use_decl_annotations_
    ThemeColor InkOn(ThemeColor const& themeInk, ThemeColor const& background) noexcept
    {
        if (themeInk.A != 0 && ContrastRatio(themeInk, background) >= 4.5)
        {
            return themeInk;
        }

        return ReadableInk(background);
    }

    _Use_decl_annotations_
    ControlColors ResolveControlColors(Control const& control, Theme const& theme) noexcept
    {
        ControlColors colors{};

        auto const hue = ResolveHue(control, theme);

        // The value can be one color on a panel whose switches are all different ones. Every
        // theme that does not name one draws each value in its control's own hue.
        auto const value = theme.ValueColor.A != 0 ? theme.ValueColor : hue;

        // A switch can be filled at rest while a knob on the same panel is bare, so the wash is
        // asked for by kind rather than read straight off the theme.
        auto const fillAtRest = FillAtRestFor(theme, FillsLikeASwitch(control.Kind));

        colors.Pipe = value;
        colors.Track = theme.TrackColor;
        colors.ArcTrack = EffectiveArcTrackColor(theme);

        for (int32_t zone = 0; zone < MeterZoneCount; ++zone)
        {
            auto const slot = std::clamp(theme.MeterSlots[static_cast<size_t>(zone)], 0, ThemeHueSlotCount - 1);

            (zone == 0 ? colors.MeterLit : zone == 1 ? colors.MeterWarn : colors.MeterHot) =
                theme.HueSlots[static_cast<size_t>(slot)];
        }

        if (theme.PlateColor.A != 0 && fillAtRest <= 0.0)
        {
            // The theme named a plate outright, which is what a raised neutral surface needs.
            colors.Plate = theme.PlateColor;
        }
        else if (fillAtRest > 0.0)
        {
            // Tonal: the plate is the deck tinted with the control's own hue. That is why the
            // tonal themes cost no extra rendering layer - a tint is a background color.
            colors.Plate = BlendOver(theme.Deck.Color, hue, fillAtRest);
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

        // A plate lifted at the top, where the theme named a second end. The tube themes all
        // want one, because a raster box is brighter where the beam started.
        colors.PlateEnd = colors.Plate;

        if (theme.PlateEndColor.A != 0 && theme.PlateColor.A != 0 && fillAtRest <= 0.0)
        {
            colors.PlateEnd = theme.PlateEndColor;
        }

        // What the resting plate actually comes out as once it is over the deck. The touch and
        // the on states are both worked out against this rather than against the plate's own
        // alpha, because a theme can have no plate at all at rest - High contrast lets the deck
        // through untouched - and a wash of nothing is still nothing.
        auto const restingTop = colors.Plate.A == 0
            ? theme.Deck.Color
            : BlendOver(theme.Deck.Color, colors.Plate, 1.0);

        auto const restingBottom = colors.PlateEnd.A == 0
            ? theme.Deck.GradientEndColor
            : BlendOver(theme.Deck.GradientEndColor, colors.PlateEnd, 1.0);

        // What the plate becomes under a finger. A flat theme has no glow to lift, so the plate
        // is the only thing left that can say a control is being held.
        if (theme.TouchFillPercent > 0)
        {
            auto const wash = std::clamp(theme.TouchFillPercent / 100.0, 0.0, 1.0);

            colors.TouchPlate = BlendOver(restingTop, hue, wash);
            colors.TouchPlate.A = 255;

            colors.TouchPlateEnd = BlendOver(restingBottom, hue, wash);
            colors.TouchPlateEnd.A = 255;
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
        {
            // A quarter strength on a dark theme, not the full hue. Nothing is saturated at
            // rest - the rim is there to say which control this is, and the value and the
            // activity are the only things allowed to be bright. A full-strength rim on every
            // control turns a busy page into a grid of neon rectangles and nothing stands out
            // at all.
            //
            // A light theme has to run it much higher, because a hairline that reads as a line
            // on near-black is not there at all on near-white. That is what the property is
            // for; it is not a taste setting.
            auto const strength = theme.RimStrengthPercent > 0
                ? theme.RimStrengthPercent / 100.0
                : RestingRimAlpha;

            colors.Rim = hue;
            colors.Rim.A = static_cast<uint8_t>(std::clamp(std::lround(hue.A * strength), 0L, 255L));
            break;
        }
        }

        // The rim comes up under a finger, whatever the rim is made of. On a theme with a
        // neutral edge the hue is reserved for the value, so the edge strengthens rather than
        // turning colored.
        //
        // !! Worked out AFTER the rim, not before it. !! Before, this read the rim while it was
        // still a default ThemeColor - opaque black - so on every theme a finger turned the rim
        // black: gone on a dark deck, a dirty line on a light one.
        if (colors.Rim.A != 0)
        {
            colors.TouchRim = colors.Rim;
            colors.TouchRim.A = std::max(colors.Rim.A, static_cast<uint8_t>(
                std::clamp(std::lround(255.0 * 0.88), 0L, 255L)));
        }

        // Activity lights up in the control's own hue on every dark theme. On a light one there
        // is nowhere for a hue to glow to - the plate is already near-white - so the theme can
        // ask for the light itself instead. A tube asks for something else again: its phosphor
        // is blue-white, or ember, or yellow-green, and none of those is either the hue or white.
        colors.Bloom = HasNamedBloomColor(theme) ? NamedBloomColor(theme) : hue;

        colors.Bloom.A = static_cast<uint8_t>(
            std::clamp(std::lround(255.0 * theme.GlowStrength / 100.0), 0L, 255L));

        // The floor under the animated value rather than a second layer. A lit thing on a tube
        // spills a little all the time, and on those themes that spill is the only thing
        // separating a control from the glass.
        colors.RestingGlow = std::clamp(theme.RestingGlowPercent / 100.0, 0.0, 1.0);

        // The bar is brightest where the value is and falls away behind it. A falloff of one
        // means the theme wants a flat bar, which is what the tonal themes and Bigwig ask for.
        // A theme with a long persistence runs the far end into its own halo instead: the top of
        // the fill is where the beam just was, and what is under it has had time to decay.
        colors.PipeEnd = theme.ValueFadesToLight && HasNamedBloomColor(theme)
            ? NamedBloomColor(theme)
            : value;

        colors.PipeEnd.A = static_cast<uint8_t>(
            std::clamp(std::lround(value.A * theme.PipeFalloff), 0L, 255L));

        // A fader's fill at the theme's own strength. On a panel where the cap position is the
        // whole value, the slot under it is only faintly lit.
        auto const fillStrength = std::clamp(theme.FaderFillPercent / 100.0, 0.0, 1.0);

        colors.Fill = colors.Pipe;
        colors.Fill.A = static_cast<uint8_t>(std::clamp(std::lround(colors.Pipe.A * fillStrength), 0L, 255L));

        colors.FillEnd = colors.PipeEnd;
        colors.FillEnd.A = static_cast<uint8_t>(
            std::clamp(std::lround(colors.PipeEnd.A * fillStrength), 0L, 255L));

        auto const sheen = EffectivePlateSheenColor(theme);

        colors.Sheen = { sheen.R, sheen.G, sheen.B, static_cast<uint8_t>(
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
            colors.ThumbLine = theme.CapLineColor.A != 0 ? theme.CapLineColor : hue;
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
        //
        // !! THE WASH GOES OVER THE RESTING PLATE, NOT OVER NOTHING. !! Laying it over nothing
        // is right only while the resting plate is dark glass. On a theme whose plate is
        // near-white the control jumped from paper to a dark translucent hue over the deck -
        // measured on Bone, 247,243,232 dropped to 193,183,166, which is a lamp going out.
        //
        // A theme can turn the fill off entirely and say it with a lamp alone, which is what
        // alpha 0 means here.
        if (theme.FillWhenOnPercent <= 0)
        {
            colors.OnPlate = { hue.R, hue.G, hue.B, 0 };
            colors.OnPlateEnd = colors.OnPlate;

            // The lamp, and the faint line of its light around the switch while it is lit.
            colors.Lamp = theme.LampColor.A != 0 ? theme.LampColor : hue;
            colors.LampRim = colors.Lamp;
            colors.LampRim.A = static_cast<uint8_t>(std::clamp(std::lround(colors.Lamp.A * 0.40), 0L, 255L));

            // A lit lamp spills its own light, not the switch's hue.
            if (theme.LampColor.A != 0 && FillsLikeASwitch(control.Kind))
            {
                colors.Bloom = { theme.LampColor.R, theme.LampColor.G, theme.LampColor.B, colors.Bloom.A };
            }
        }
        else
        {
            auto const onFill = std::clamp(theme.FillWhenOnPercent / 100.0, 0.0, 1.0);

            // The bottom carries half as much at a third's worth of fill, which is the lit top
            // edge every theme is drawn with. It catches up as the fill grows, so a tab that IS
            // its color when it is on is that color top to bottom rather than fading out.
            auto const bottomFill = onFill <= 0.5
                ? onFill * 0.5
                : 0.25 + (onFill - 0.5) * 1.5;

            colors.OnPlate = BlendOver(restingTop, hue, onFill);
            colors.OnPlate.A = 255;

            colors.OnPlateEnd = BlendOver(restingBottom, hue, bottomFill);
            colors.OnPlateEnd.A = 255;

            // Paler for a tab lit from behind, deeper for a colored button on paper.
            if (theme.OnLiftPercent != 0)
            {
                auto const lift = std::clamp(theme.OnLiftPercent / 100.0, -1.0, 1.0);

                constexpr ThemeColor white{ 255, 255, 255, 255 };
                constexpr ThemeColor black{ 0, 0, 0, 255 };

                auto const& toward = lift > 0.0 ? white : black;
                auto const amount = std::fabs(lift);

                colors.OnPlate = BlendOver(colors.OnPlate, toward, amount);
                colors.OnPlateEnd = BlendOver(colors.OnPlateEnd, toward, lift > 0.0 ? amount * 0.5 : amount);
            }
        }

        colors.OnRim = hue;
        colors.OnRim.A = static_cast<uint8_t>(std::clamp(std::lround(hue.A * 0.90), 0L, 255L));

        // The label sits on the plate where there is one, and on the deck where there is not.
        // A theme can name its ink instead of having it measured, because a phosphor's color is
        // a property of the tube rather than something to work out from the glass.
        colors.Label = theme.InkColor.A != 0
            ? theme.InkColor
            : ReadableInk(
                colors.Plate.A >= 128
                ? BlendOver(theme.Deck.Color, colors.Plate, 1.0)
                : theme.Deck.Color);

        // Derived from the hue until a panel arrived whose every knob points in the section
        // color. That is the mirror of the white-and-black rule: anything worked out from the
        // CONTROL'S HUE eventually meets a theme where it is not the hue.
        colors.Pointer = theme.PointerColor.A != 0
            ? theme.PointerColor
            : (theme.Rim == RimSource::NeutralEdge ? colors.Label : hue);

        colors.Marks = colors.Label;
        colors.Marks.A = MarkAlpha;

        // A name inside a switch sits on the plate, and a lit plate can be a different color
        // altogether, so it is measured against both.
        auto const restingMiddle = BlendOver(restingTop, restingBottom, 0.5);

        colors.SwitchInk = InkOn(theme.InkColor, restingMiddle);
        colors.SwitchInkOn = colors.OnPlate.A == 0
            ? colors.SwitchInk
            : InkOn(theme.InkColor, BlendOver(colors.OnPlate, colors.OnPlateEnd, 0.5));

        // A knob's face is the plate unless the theme turned one of its own.
        if (theme.KnobFaceColor.A != 0)
        {
            colors.KnobFace = theme.KnobFaceColor;
            colors.KnobFaceEnd = theme.KnobFaceEndColor.A != 0 ? theme.KnobFaceEndColor : theme.KnobFaceColor;
        }
        else
        {
            colors.KnobFace = colors.Plate;
            colors.KnobFaceEnd = colors.PlateEnd;
        }

        if (theme.KnobCapColor.A != 0)
        {
            colors.KnobCap = theme.KnobCapColor;
            colors.KnobCapEnd = theme.KnobCapEndColor.A != 0 ? theme.KnobCapEndColor : theme.KnobCapColor;
        }

        // Printed on the panel, so it is the ink rather than the barely-there marks inside a
        // control.
        if (theme.KnobTickCount > 0)
        {
            colors.KnobTick = colors.Label;
            colors.KnobTick.A = static_cast<uint8_t>(std::clamp(std::lround(colors.Label.A * 0.60), 0L, 255L));
        }

        auto const withStrength = [](ThemeColor color, int32_t percent) noexcept
            {
                color.A = static_cast<uint8_t>(
                    std::clamp(std::lround(255.0 * std::clamp(percent, 0, 100) / 100.0), 0L, 255L));

                return color;
            };

        colors.Well = theme.WellColor;

        if (theme.RecessShadePercent > 0)
        {
            colors.Recess = withStrength(theme.ShadowColor, theme.RecessShadePercent);
        }

        if (theme.PlateShadePercent > 0)
        {
            colors.PlateShade = withStrength(theme.ShadowColor, theme.PlateShadePercent);
        }

        if (theme.PlateHighlightPercent > 0)
        {
            colors.PlateHighlight = withStrength(EffectivePlateSheenColor(theme), theme.PlateHighlightPercent);
        }

        colors.PanelOutline = theme.PanelOutlineColor.A != 0 ? theme.PanelOutlineColor : colors.Rim;

        colors.KeyWhite = EffectiveKeyWhiteColor(theme);
        colors.KeyBlack = EffectiveKeyBlackColor(theme);

        return colors;
    }

    _Use_decl_annotations_
    bool UsesLampRing(Theme const& theme, double controlWidth, double controlHeight) noexcept
    {
        return theme.ValueIndicator == ValueIndicatorStyle::SegmentedLamps &&
            std::min(controlWidth, controlHeight) >= theme.MinimumLampRingSize;
    }
}
