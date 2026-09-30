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
#include <cstdlib>

namespace glass
{
    namespace
    {
        uint8_t Mix(_In_ uint8_t base, _In_ uint8_t over, _In_ double amount) noexcept
        {
            return static_cast<uint8_t>(std::clamp(
                std::lround(base + (over - base) * amount), 0L, 255L));
        }

        // Tick marks and center dots are orientation, not information. They have to be findable
        // when looked for and invisible when not.
        constexpr uint8_t MarkAlpha = 46;

        // A rule the theme does not name is the ink at a sixth, which is the design sheet's own.
        constexpr double DerivedRuleStrength = 0.16;

        // A printed ring of marks round a knob is the ink at six tenths.
        constexpr double KnobTickStrength = 0.60;

        ThemeColor AtStrength(_In_ ThemeColor color, _In_ double strength) noexcept
        {
            color.A = static_cast<uint8_t>(std::clamp(std::lround(color.A * std::clamp(strength, 0.0, 1.0)), 0L, 255L));

            return color;
        }

        // A cap in the neutral: lifted where the light catches it and shaded at the bottom, with
        // a line through it dark enough to read on cream.
        ThemeColor NeutralCapTop(_In_ ThemeColor const& neutral) noexcept
        {
            auto top = BlendOver(neutral, ThemeColor{ 255, 255, 255, 255 }, 0.45);
            top.A = 255;

            return top;
        }

        ThemeColor NeutralCapBottom(_In_ ThemeColor const& neutral) noexcept
        {
            auto bottom = BlendOver(neutral, ThemeColor{ 0, 0, 0, 255 }, 0.12);
            bottom.A = 255;

            return bottom;
        }

        ThemeColor NeutralCapLine(_In_ ThemeColor const& neutral) noexcept
        {
            auto line = BlendOver(neutral, ThemeColor{ 0, 0, 0, 255 }, 0.74);
            line.A = 255;

            return line;
        }

        ThemeColor Opaque(_In_ ThemeColor color) noexcept
        {
            color.A = 255;

            return color;
        }

        // A color scaled channel by channel, which is how the second key on a keyboard is the
        // first one in a darker plastic.
        ThemeColor Scaled(_In_ ThemeColor const& color, _In_ std::array<double, 3> const& ratio) noexcept
        {
            auto const scale = [](uint8_t channel, double by) noexcept
                {
                    return static_cast<uint8_t>(std::clamp(std::lround(channel * by), 0L, 255L));
                };

            return { scale(color.R, ratio[0]), scale(color.G, ratio[1]), scale(color.B, ratio[2]), 255 };
        }

        ThemeColor Darker(_In_ ThemeColor const& color, _In_ int32_t levels) noexcept
        {
            auto const drop = [levels](uint8_t channel) noexcept
                {
                    return static_cast<uint8_t>(std::clamp(static_cast<int32_t>(channel) - levels, 0, 255));
                };

            return { drop(color.R), drop(color.G), drop(color.B), color.A };
        }

        // How much darker a key's top gets when it is held down, further from the light.
        constexpr int32_t KeycapHeldLevels = 8;

        // An unlit meter segment is its zone's own color turned right down.
        constexpr double MeterUnlitStrength = 0.16;
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
        case ControlKind::Steps:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool FillsLikeASwitch(ControlKind kind) noexcept
    {
        return IsSwitchControl(kind) && kind != ControlKind::Lfo && kind != ControlKind::Steps;
    }

    _Use_decl_annotations_
    bool IsTurnedOrSlid(ControlKind kind) noexcept
    {
        switch (kind)
        {
        case ControlKind::Knob:
        case ControlKind::Fader:
        case ControlKind::XYPad:
        case ControlKind::Joystick:
        case ControlKind::Ribbon:
        case ControlKind::Turntable:
        case ControlKind::Wheel:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    double FillAtRestForKind(Theme const& theme, ControlKind kind) noexcept
    {
        if (kind == ControlKind::Pad && theme.PadFillAtRest >= 0.0)
        {
            return theme.PadFillAtRest;
        }

        return FillAtRestFor(theme, FillsLikeASwitch(kind));
    }

    _Use_decl_annotations_
    int32_t FillWhenOnFor(Theme const& theme, ControlKind kind) noexcept
    {
        if (kind == ControlKind::Pad && theme.PadFillWhenOnPercent >= 0)
        {
            return theme.PadFillWhenOnPercent;
        }

        if (kind == ControlKind::Lamp && theme.LampFillWhenOnPercent >= 0)
        {
            return theme.LampFillWhenOnPercent;
        }

        return theme.FillWhenOnPercent;
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
    ThemeColor InkOn(ThemeColor const& preferred, ThemeColor const& fallback, ThemeColor const& background) noexcept
    {
        if (preferred.A != 0 && ContrastRatio(preferred, background) >= 4.5)
        {
            return preferred;
        }

        return InkOn(fallback, background);
    }

    _Use_decl_annotations_
    bool IsKeycap(Theme const& theme, ControlKind kind) noexcept
    {
        return theme.SwitchShape == SwitchShapeStyle::Keycap &&
            theme.PlateColor.A != 0 &&
            FillsLikeASwitch(kind) &&
            kind != ControlKind::Lamp;
    }

    _Use_decl_annotations_
    bool IsRoundSwitch(Theme const& theme, ControlKind kind) noexcept
    {
        if (theme.SwitchShape != SwitchShapeStyle::Round)
        {
            return false;
        }

        return kind == ControlKind::Button ||
            kind == ControlKind::Toggle ||
            (kind == ControlKind::Pad && theme.PadsFollowSwitchShape);
    }

    _Use_decl_annotations_
    bool LatchesOn(ControlKind kind) noexcept
    {
        return kind == ControlKind::Toggle || kind == ControlKind::PageTab;
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
        auto const fillAtRest = FillAtRestForKind(theme, control.Kind);

        // Anything pressed or read rather than turned or slid. A theme can light a knob's ring
        // and a fader's frame all the time while its buttons stay black until they are on.
        auto const pressedOrRead = !IsTurnedOrSlid(control.Kind) &&
            control.Kind != ControlKind::Panel &&
            control.Kind != ControlKind::Line;

        // One row of cream caps on a panel of black ones. A theme with no neutral has nothing to
        // put on the cap, so the control stays what it was.
        auto const neutralCap = theme.NeutralCaps &&
            control.HueSlot == NeutralSlot &&
            HasNeutralColor(theme);

        auto const fillWhenOn = FillWhenOnFor(theme, control.Kind);

        colors.Pipe = value;
        colors.Track = theme.TrackColor;

        // A ring of its own light, or the one track every knob shares.
        colors.ArcTrack = theme.ArcTrackHuePercent > 0
            ? AtStrength(hue, theme.ArcTrackHuePercent / 100.0)
            : EffectiveArcTrackColor(theme);

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
            // tonal themes cost no extra rendering layer - a tint is a background color. A theme
            // can ask for the tint over its plate instead, so a pad is its color on the same
            // cream as everything around it rather than on the page.
            auto const under = theme.RestTintOnPlate && theme.PlateColor.A != 0
                ? Opaque(theme.PlateColor)
                : theme.Deck.Color;

            colors.Plate = BlendOver(under, hue, fillAtRest);
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

        // A switch on the neutral slot wears the neutral as its cap, whatever the plate is.
        if (neutralCap && FillsLikeASwitch(control.Kind))
        {
            colors.Plate = NeutralCapTop(theme.NeutralColor);
            colors.PlateEnd = NeutralCapBottom(theme.NeutralColor);
        }

        // A key: its skirt is the plate, and its dished top is set into it. The key on the
        // neutral slot is the other key on the keyboard, the same shape in a second plastic,
        // so all four of its colors are the first key's scaled to the neutral.
        auto const keycap = IsKeycap(theme, control.Kind);
        auto const keycapFader = theme.SwitchShape == SwitchShapeStyle::Keycap &&
            theme.PlateColor.A != 0 &&
            control.Kind == ControlKind::Fader;

        if (keycap || keycapFader)
        {
            auto skirtTop = Opaque(keycapFader ? theme.ThumbColor : theme.PlateColor);
            auto skirtEnd = Opaque(keycapFader
                ? theme.ThumbEndColor
                : (theme.PlateEndColor.A != 0 ? theme.PlateEndColor : theme.PlateColor));

            constexpr ThemeColor white{ 255, 255, 255, 255 };

            auto faceTop = theme.KeycapTopColor.A != 0
                ? Opaque(theme.KeycapTopColor)
                : Opaque(BlendOver(Opaque(theme.PlateColor), white, 0.08));

            auto faceEnd = theme.KeycapTopEndColor.A != 0
                ? Opaque(theme.KeycapTopEndColor)
                : Opaque(BlendOver(Opaque(theme.PlateColor), white, 0.45));

            auto sheen = theme.PlateHighlightPercent / 100.0;
            auto outlineShare = 0.65;

            if (neutralCap)
            {
                auto const middle = BlendOver(skirtTop, skirtEnd, 0.4);

                auto const ratio = [](uint8_t to, uint8_t from) noexcept
                    {
                        return from == 0 ? 1.0 : static_cast<double>(to) / from;
                    };

                std::array<double, 3> const by
                {
                    ratio(theme.NeutralColor.R, middle.R),
                    ratio(theme.NeutralColor.G, middle.G),
                    ratio(theme.NeutralColor.B, middle.B),
                };

                skirtTop = Scaled(skirtTop, by);
                skirtEnd = Scaled(skirtEnd, by);
                faceTop = Scaled(faceTop, by);
                faceEnd = Scaled(faceEnd, by);

                // A darker plastic catches less of the light and shows more of its edge.
                sheen *= 0.5;
                outlineShare = 0.85;
            }

            if (keycap)
            {
                colors.Plate = skirtTop;
                colors.PlateEnd = skirtEnd;
            }

            colors.KeycapTop = faceTop;
            colors.KeycapTopEnd = faceEnd;
            colors.KeycapTopHeld = Darker(faceTop, KeycapHeldLevels);
            colors.KeycapTopHeldEnd = Darker(faceEnd, KeycapHeldLevels);

            auto const edge = theme.NeutralRimColor;

            colors.KeycapOutline = AtStrength(edge, outlineShare);
            colors.KeycapSheen = AtStrength(EffectivePlateSheenColor(theme), sheen);
            colors.KeycapFoot = AtStrength(theme.ShadowColor, theme.PlateShadePercent / 100.0);
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
            //
            // Zero is no rim at all. It used to fall back to a quarter, so a slider taken to the
            // bottom still drew one.
            auto const percent = pressedOrRead && theme.SwitchRimStrengthPercent >= 0
                ? theme.SwitchRimStrengthPercent
                : theme.RimStrengthPercent;

            auto const strength = std::clamp(percent, 0, 100) / 100.0;

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
        auto const glowPercent = pressedOrRead && theme.SwitchRestingGlowPercent >= 0
            ? theme.SwitchRestingGlowPercent
            : theme.RestingGlowPercent;

        colors.RestingGlow = std::clamp(glowPercent / 100.0, 0.0, 1.0);

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

        if (neutralCap && theme.Thumb != ThumbStyle::None && !keycapFader)
        {
            colors.Thumb = NeutralCapTop(theme.NeutralColor);
            colors.ThumbEnd = NeutralCapBottom(theme.NeutralColor);
            colors.ThumbLine = NeutralCapLine(theme.NeutralColor);
        }

        // A fader's cap is a small key: the neutral one is the other plastic, like a switch.
        if (keycapFader && theme.Thumb != ThumbStyle::None && neutralCap)
        {
            auto const middle = BlendOver(Opaque(theme.ThumbColor), Opaque(theme.ThumbEndColor), 0.4);

            auto const ratio = [](uint8_t to, uint8_t from) noexcept
                {
                    return from == 0 ? 1.0 : static_cast<double>(to) / from;
                };

            std::array<double, 3> const by
            {
                ratio(theme.NeutralColor.R, middle.R),
                ratio(theme.NeutralColor.G, middle.G),
                ratio(theme.NeutralColor.B, middle.B),
            };

            colors.Thumb = Scaled(Opaque(theme.ThumbColor), by);
            colors.ThumbEnd = Scaled(Opaque(theme.ThumbEndColor), by);
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
        if (fillWhenOn <= 0)
        {
            colors.OnPlate = { hue.R, hue.G, hue.B, 0 };
            colors.OnPlateEnd = colors.OnPlate;

            // The lamp, and the faint line of its light around the switch while it is lit. A
            // lamp the color of the cream cap it sits in would not be there at all, so a neutral
            // cap lights the theme's first lamp.
            colors.Lamp = theme.LampColor.A != 0
                ? theme.LampColor
                : (neutralCap ? theme.HueSlots[0] : hue);

            colors.LampRim = colors.Lamp;
            colors.LampRim.A = static_cast<uint8_t>(std::clamp(std::lround(colors.Lamp.A * 0.40), 0L, 255L));

            // A lit lamp spills its own light, not the switch's hue.
            if ((theme.LampColor.A != 0 || neutralCap) && FillsLikeASwitch(control.Kind))
            {
                colors.Bloom = { colors.Lamp.R, colors.Lamp.G, colors.Lamp.B, colors.Bloom.A };
            }
        }
        else
        {
            auto const onFill = std::clamp(fillWhenOn / 100.0, 0.0, 1.0);

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

            // A switch that stays on is checkered rather than colored, and one that only goes
            // on while it is held is simply pressed. A pad and a lamp still light in their color.
            if (theme.Latch == LatchStyle::Checkerboard &&
                control.Kind != ControlKind::Pad &&
                control.Kind != ControlKind::Lamp &&
                FillsLikeASwitch(control.Kind))
            {
                if (LatchesOn(control.Kind))
                {
                    auto const light = EffectiveBevelHighlightColor(theme);

                    colors.OnPlate = BlendOver(restingTop, light, 0.5);
                    colors.OnPlate.A = 255;
                    colors.OnPlateEnd = BlendOver(restingBottom, light, 0.5);
                    colors.OnPlateEnd.A = 255;
                }
                else
                {
                    colors.OnPlate = restingTop;
                    colors.OnPlate.A = 255;
                    colors.OnPlateEnd = restingBottom;
                    colors.OnPlateEnd.A = 255;
                }
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
        // altogether, so it is measured against both. At rest the theme's ink comes first; lit,
        // the ink the theme names for a lit switch does.
        auto const restingMiddle = BlendOver(restingTop, restingBottom, 0.5);

        colors.SwitchInk = InkOn(theme.InkColor, theme.OnInkColor, restingMiddle);
        colors.SwitchInkOn = colors.OnPlate.A == 0
            ? colors.SwitchInk
            : InkOn(theme.OnInkColor, theme.InkColor, BlendOver(colors.OnPlate, colors.OnPlateEnd, 0.5));

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

        // A cap in the knob's own color: lit a little at the top, deeper at its edge. The line on
        // it is the theme's pointer where that reads on this cap, and otherwise white or near
        // black, whichever does, the way a name on a switch is chosen.
        if (theme.KnobCapFromHue && control.Kind == ControlKind::Knob)
        {
            constexpr ThemeColor white{ 255, 255, 255, 255 };
            constexpr ThemeColor black{ 0, 0, 0, 255 };
            constexpr ThemeColor nearBlack{ 0x11, 0x11, 0x11, 255 };

            auto cap = hue;
            cap.A = 255;

            colors.KnobCap = BlendOver(cap, white, 0.12);
            colors.KnobCapEnd = BlendOver(cap, black, 0.28);

            auto const capMiddle = BlendOver(colors.KnobCap, colors.KnobCapEnd, 0.5);

            if (theme.PointerColor.A == 0 || ContrastRatio(theme.PointerColor, capMiddle) < 3.0)
            {
                colors.Pointer = ContrastRatio(white, capMiddle) >= ContrastRatio(nearBlack, capMiddle)
                    ? white
                    : nearBlack;
            }
        }

        // Printed on the panel, so it is the ink rather than the barely-there marks inside a
        // control.
        if (theme.KnobTickCount > 0)
        {
            colors.KnobTick = AtStrength(colors.Label, KnobTickStrength);
        }

        // A fader's scale printed beside its slot the way the ring is printed round a knob.
        colors.FaderTick = theme.FaderScalePercent > 0
            ? AtStrength(colors.Label, theme.FaderScalePercent / 100.0)
            : colors.Marks;

        // Ink by surface. A section is printed in its own ink where the theme names one; an inset
        // and the deck keep the theme's.
        colors.SectionLabel = theme.SectionInkColor.A != 0 ? theme.SectionInkColor : colors.Label;

        if (theme.KnobTickCount > 0)
        {
            colors.SectionKnobTick = AtStrength(colors.SectionLabel, KnobTickStrength);
        }

        colors.SectionFaderTick = theme.FaderScalePercent > 0
            ? AtStrength(colors.SectionLabel, theme.FaderScalePercent / 100.0)
            : colors.Marks;

        colors.Rule = theme.RuleColor.A != 0
            ? theme.RuleColor
            : AtStrength(colors.Label, DerivedRuleStrength);

        colors.SectionRule = theme.SectionInkColor.A != 0
            ? ThemeColor{ theme.SectionInkColor.R, theme.SectionInkColor.G, theme.SectionInkColor.B, colors.Rule.A }
            : colors.Rule;

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

        colors.LampHolder = theme.LampHolderColor;
        colors.RecessLip = theme.RecessLipColor;

        // Every segment is there when it is out, so a meter reads as a row of lights.
        auto const unlit = [&theme](ThemeColor const& zone) noexcept
            {
                return theme.MeterUnlitColor.A != 0
                    ? theme.MeterUnlitColor
                    : AtStrength(zone, MeterUnlitStrength);
            };

        colors.MeterLitOff = unlit(colors.MeterLit);
        colors.MeterWarnOff = unlit(colors.MeterWarn);
        colors.MeterHotOff = unlit(colors.MeterHot);

        // Behind smoked plastic a light keeps its own color, even on a theme that prints every
        // other value in one ink.
        colors.WellValue = theme.WellFillsControl && theme.ValueColor.A != 0 ? hue : value;

        // A number in a window can be printed in an ink of its own. Six colors dark enough to
        // read on a light panel are too dark for small figures on black.
        colors.WellInk = theme.WellInkColor.A != 0 ? Opaque(theme.WellInkColor) : colors.WellValue;

        if (theme.BevelPixels > 0)
        {
            colors.BevelHighlight = Opaque(EffectiveBevelHighlightColor(theme));
            colors.BevelLight = Opaque(EffectiveBevelLightColor(theme));
            colors.BevelShadow = Opaque(EffectiveBevelShadowColor(theme));
            colors.BevelDark = Opaque(EffectiveBevelDarkColor(theme));
        }

        if (theme.ValueCorePercent > 0)
        {
            colors.ValueCore = Opaque(BlendOver(
                Opaque(value), ThemeColor{ 255, 255, 255, 255 }, std::clamp(theme.ValueCorePercent, 0, 100) / 100.0));
        }

        if (theme.FlarePercent > 0)
        {
            colors.Flare = theme.FlareColor.A != 0 ? Opaque(theme.FlareColor) : Opaque(value);
        }

        return colors;
    }

    _Use_decl_annotations_
    bool UsesLampRing(Theme const& theme, double controlWidth, double controlHeight) noexcept
    {
        return theme.ValueIndicator == ValueIndicatorStyle::SegmentedLamps &&
            std::min(controlWidth, controlHeight) >= theme.MinimumLampRingSize;
    }

    namespace
    {
        // How much of its role's color a resting pad carries, and its rim, in the order of
        // PadRole: out of the key, in it, and the root. Stronger than anything else on a
        // resting surface, because on a grid of pads the colors ARE the information: they are
        // how a player finds the key without looking for it.
        constexpr double PadRestFillStrength[3]{ 0.16, 0.38, 0.70 };
        constexpr double PadRestRimStrength[3]{ 0.30, 0.60, 0.90 };

        // A pad under a finger is its own color lifted toward white, so it lights rather than
        // turning into a different color.
        constexpr double PadLitTowardWhite = 0.35;

        constexpr double PadDeadStrength = 0.06;

        // The outline a pad gets when its own rim would vanish into the plate: a white pad typed
        // in on a white theme.
        constexpr double PadEdgeStrength = 0.30;

        // How far apart two resting pads have to land, in the largest of their three channels,
        // before a player can tell their roles apart at a glance. The fixed strengths above miss
        // it on a few shipped themes: on Cathode every slot is the same phosphor, and on Bone
        // the hues are muted enough to sit close to the dark pads outside the key.
        constexpr int32_t PadRoleSeparation = 24;

        // How far the key's pads and the root may be raised to get that far apart. Past these a
        // resting grid starts to look like one that is being played.
        constexpr double PadInKeyStrengthLimit = 0.56;
        constexpr double PadRootStrengthLimit = 0.90;
        constexpr double PadStrengthStep = 0.04;

        int32_t LargestChannelGap(_In_ ThemeColor const& a, _In_ ThemeColor const& b) noexcept
        {
            return std::max({
                std::abs(static_cast<int32_t>(a.R) - static_cast<int32_t>(b.R)),
                std::abs(static_cast<int32_t>(a.G) - static_cast<int32_t>(b.G)),
                std::abs(static_cast<int32_t>(a.B) - static_cast<int32_t>(b.B)) });
        }

        // ReadableInk's candidate, pushed toward pure white or pure black only as far as a note
        // name needs to clear 4.5 : 1. A pad's fill can land in the middle of the gray scale,
        // where neither softened candidate gets past about 4.1. The pure color on the same side
        // always does, so the loop always ends on a readable ink.
        ThemeColor PadInk(_In_ ThemeColor const& background) noexcept
        {
            auto const ink = ReadableInk(background);

            if (ContrastRatio(ink, background) >= 4.5)
            {
                return ink;
            }

            auto const extreme = ink.R > 127 ? ThemeColor{ 255, 255, 255, 255 } : ThemeColor{ 0, 0, 0, 255 };

            for (int32_t step = 1; step < 10; ++step)
            {
                auto candidate = BlendOver(ink, extreme, step / 10.0);
                candidate.A = 255;

                if (ContrastRatio(candidate, background) >= 4.5)
                {
                    return candidate;
                }
            }

            return extreme;
        }
    }

    _Use_decl_annotations_
    PadColors ResolvePadColors(Control const& control, Theme const& theme, ThemeColor const& behind) noexcept
    {
        PadColors colors{};

        auto const& spec = control.Pads;

        ThemeColor parsed{};

        auto const namedIn = TryParseColor(spec.InKeyColor, parsed);
        auto const inKey = namedIn ? parsed : ResolveHue(control, theme);

        // Across the palette from the control's own slot, so the root can never come out the
        // same color as the rest of the key, and a theme swap still reaches it.
        ThemeColor root{};

        auto const namedRoot = TryParseColor(spec.RootColor, parsed);

        if (namedRoot)
        {
            root = parsed;
        }
        else
        {
            auto const slot = control.HueSlot >= 0 && control.HueSlot < ThemeHueSlotCount ? control.HueSlot : 0;

            root = theme.HueSlots[static_cast<size_t>((slot + ThemeHueSlotCount / 2) % ThemeHueSlotCount)];
        }

        // The theme's one un-hued color, or the deck's own ink: white pads on a dark theme and
        // dark ones on a light theme, the way the pads outside the key are on the hardware.
        auto const namedOut = TryParseColor(spec.OutOfKeyColor, parsed);
        auto const outOfKey = namedOut
            ? parsed
            : (HasNeutralColor(theme) ? theme.NeutralColor : ReadableInk(behind));

        ThemeColor pressed{};

        auto const namedPressed = TryParseColor(spec.PressedColor, pressed);

        // Where a resting pad of a color and a strength actually lands on the plate, with the
        // fill's alpha rounded to a byte the way it is drawn. Measuring the unrounded blend put
        // a name at 4.48 : 1 that this said was 4.5.
        auto const landed = [&behind](ThemeColor color, double strength) noexcept
            {
                color.A = 255;

                return BlendOver(behind, AtStrength(color, strength), 1.0);
            };

        // A color somebody typed in is the color of the pad, not a tint of it: white means white.
        std::array<bool, 3> const named{ namedOut, namedIn, namedRoot };
        std::array<double, 3> strength{};

        for (size_t index = 0; index < strength.size(); ++index)
        {
            strength[index] = named[index] ? 1.0 : PadRestFillStrength[index];
        }

        auto const outLanded = landed(outOfKey, strength[0]);

        // The key's pads come up until they clear the pads outside it.
        while (!namedIn && strength[1] < PadInKeyStrengthLimit &&
            LargestChannelGap(outLanded, landed(inKey, strength[1])) < PadRoleSeparation)
        {
            strength[1] = std::min(strength[1] + PadStrengthStep, PadInKeyStrengthLimit);
        }

        auto const inLanded = landed(inKey, strength[1]);

        auto const rootGap = [&](ThemeColor const& color, double rootStrength) noexcept
            {
                auto const at = landed(color, rootStrength);

                return std::min(LargestChannelGap(at, inLanded), LargestChannelGap(at, outLanded));
            };

        // Then the root, until it clears both. Brighter first, because that keeps the color the
        // theme put across the palette.
        while (!namedRoot && strength[2] < PadRootStrengthLimit &&
            rootGap(root, strength[2]) < PadRoleSeparation)
        {
            strength[2] = std::min(strength[2] + PadStrengthStep, PadRootStrengthLimit);
        }

        // A theme whose opposite slots are the same color at the same brightness gets whichever
        // slot lands furthest from the rest. A root somebody typed in is theirs to keep.
        if (!namedRoot && rootGap(root, strength[2]) < PadRoleSeparation)
        {
            auto bestGap = rootGap(root, strength[2]);

            for (auto const& candidate : theme.HueSlots)
            {
                auto const gap = rootGap(candidate, strength[2]);

                if (gap > bestGap)
                {
                    root = candidate;
                    bestGap = gap;
                }
            }
        }

        std::array<ThemeColor, 3> const roles{ outOfKey, inKey, root };

        // Rimmed the way the theme rims its own pad control: Supersaw's molding has no rim.
        auto const rimmed = ResolveControlColors(control, theme).Rim.A != 0;

        for (size_t index = 0; index < roles.size(); ++index)
        {
            auto solid = roles[index];
            solid.A = 255;

            // Solid, because a pad stands up off the plate and casts a shadow onto it.
            colors.RestFill[index] = landed(solid, strength[index]);
            colors.RestRim[index] = rimmed
                ? AtStrength(solid, std::max(PadRestRimStrength[index], strength[index]))
                : ThemeColor{ 0, 0, 0, 0 };
            colors.RestInk[index] = PadInk(colors.RestFill[index]);

            auto const fillShows = LargestChannelGap(colors.RestFill[index], behind) >= PadRoleSeparation;
            auto const rimShows = colors.RestRim[index].A != 0 &&
                LargestChannelGap(BlendOver(behind, colors.RestRim[index], 1.0), behind) >= PadRoleSeparation;

            if (!fillShows && !rimShows)
            {
                colors.RestRim[index] = AtStrength(ReadableInk(behind), PadEdgeStrength);
            }

            auto lit = namedPressed ? pressed : BlendOver(solid, ThemeColor{ 255, 255, 255, 255 }, PadLitTowardWhite);
            lit.A = 255;

            // A pad already close to white has nowhere brighter to go, so it darkens instead of
            // looking the same under a finger.
            if (!namedPressed && LargestChannelGap(lit, colors.RestFill[index]) < PadRoleSeparation)
            {
                lit = BlendOver(solid, ThemeColor{ 0, 0, 0, 255 }, PadLitTowardWhite);
                lit.A = 255;
            }

            colors.LitFill[index] = lit;
            colors.LitInk[index] = PadInk(lit);
        }

        colors.LitRim = AtStrength(ReadableInk(behind), 0.9);

        auto dead = outOfKey;
        dead.A = 255;

        colors.DeadFill = AtStrength(dead, PadDeadStrength);

        return colors;
    }
}
