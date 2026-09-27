// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "ThemeModel.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        constexpr ThemeColor Rgb(_In_ uint32_t value) noexcept
        {
            return
            {
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF),
                255
            };
        }

        // A deck is lit from above. The theme names ONE color and gets both ends of that: a
        // lifted top and a shaded floor.
        //
        // Deriving the top upward matters. Deriving only downward leaves the lit end at the
        // color the theme named, and for a near-black deck that puts the whole page within a few
        // values of the editor's work area - the page stops reading as an object at all, which
        // is exactly what the first attempt at this looked like.
        //
        // Both ends move by a fraction of the room they have rather than by a fixed step,
        // because a near-white deck that fell as far as a near-black one would look dirty.
        ThemeColor DeckLit(_In_ ThemeColor const& base) noexcept
        {
            auto const lift = [](uint8_t channel) noexcept
                {
                    auto const value = static_cast<int32_t>(std::lround(
                        channel + (255.0 - channel) * 0.07));

                    return static_cast<uint8_t>(std::clamp(value, 0, 255));
                };

            return { lift(base.R), lift(base.G), lift(base.B), base.A };
        }

        // How far a deck falls from its lit end to its floor. A dark deck absorbs and falls a
        // long way; a light one only shades.
        double DeckFallOff(_In_ ThemeColor const& base) noexcept
        {
            auto const brightness = (base.R * 0.299 + base.G * 0.587 + base.B * 0.114) / 255.0;

            return 0.06 + 0.35 * (1.0 - brightness);
        }

        ThemeColor ShadeBy(_In_ ThemeColor const& base, _In_ double amount) noexcept
        {
            auto const drop = [amount](uint8_t channel) noexcept
                {
                    auto const value = static_cast<int32_t>(std::lround(channel * (1.0 - amount)));

                    return static_cast<uint8_t>(std::clamp(value, 0, 255));
                };

            return { drop(base.R), drop(base.G), drop(base.B), base.A };
        }

        ThemeColor DeckFloor(_In_ ThemeColor const& base) noexcept
        {
            return ShadeBy(base, DeckFallOff(base));
        }

        // The empty part of a slot, which is a groove: below the surface it is cut into. Derived
        // rather than authored so it can never drift away from its deck the way six hand-picked
        // track colors did the moment the decks were lit.
        ThemeColor DeckGroove(_In_ ThemeColor const& lit) noexcept
        {
            return ShadeBy(lit, 0.55);
        }

        // The glass a plate is cut from: the deck's own color lifted toward the light, far
        // enough to still separate from it where the deck gradient is at its brightest. A plate
        // DARKER than the deck reads as a hole cut in the surface rather than an object sitting
        // on it, and that is what the whole language rests on.
        ThemeColor GlassFrom(_In_ ThemeColor const& base) noexcept
        {
            auto const lift = [](uint8_t c) noexcept
                {
                    return static_cast<uint8_t>(std::clamp(
                        std::lround(c + std::max(6.0, (255.0 - c) * 0.045)), 0L, 255L));
                };

            return { lift(base.R), lift(base.G), lift(base.B), 255 };
        }

        Theme MakeDarkTheme(
            _In_ std::wstring name,
            _In_ uint32_t deck,
            _In_ std::array<uint32_t, ThemeHueSlotCount> const& hues) noexcept
        {
            Theme theme{};

            theme.Name = std::move(name);
            theme.IsBuiltIn = true;

            // A deck is lit from above: lighter at the top, darker at the floor. A flat color
            // over a large area reads as a hole rather than as a surface, which is the single
            // biggest difference between the design comps and a first pass at them.
            //
            // The theme names the color it is recognized by; both ends are derived from it, so
            // a theme file still only has to name one.
            theme.Deck.Kind = DeckKind::Gradient;
            theme.Deck.Color = DeckLit(Rgb(deck));
            theme.Deck.GradientEndColor = DeckFloor(Rgb(deck));

            // Under the hand the plate lifts toward the control's own color. The design sheet
            // draws a glow around it as well, and the glow is the part people notice, but the
            // plate is the part that is always there to notice.
            theme.TouchFillPercent = 22;

            // A theme can still name its own track; this is what it gets if it does not.
            theme.TrackColor = DeckGroove(theme.Deck.Color);

            // The glass a plate is cut from: the deck's own color taken well down, so it keeps
            // the theme's cast instead of going neutral black.
            theme.GlassColor = GlassFrom(Rgb(deck));

            for (size_t i = 0; i < hues.size(); ++i)
            {
                theme.HueSlots[i] = Rgb(hues[i]);
            }

            return theme;
        }

        // A cathode ray tube. The three of them share a shape, and it is not the shape of any of
        // the other themes: the deck is named at both ends rather than derived, the plate is a
        // lifted gradient instead of a sheen, there is no drop shadow at all, and what separates
        // a control from the glass is the light it spills rather than a difference in value.
        //
        // Measured plate against the middle of its own deck: Cathode 1.12 : 1, Amber 1.11 : 1,
        // Terminal Green 1.22 : 1. None of that is a value difference, so RestingGlowPercent is
        // structure on all three and a theme editor has to refuse to let it reach zero.
        Theme MakeTubeTheme(
            _In_ std::wstring name,
            _In_ uint32_t deckTop,
            _In_ uint32_t deckFloor,
            _In_ uint32_t vignetteFloor,
            _In_ uint32_t plateTop,
            _In_ uint32_t plateFloor,
            _In_ uint32_t bloom,
            _In_ std::array<uint32_t, ThemeHueSlotCount> const& hues) noexcept
        {
            Theme theme{};

            theme.Name = std::move(name);
            theme.IsBuiltIn = true;

            for (size_t i = 0; i < hues.size(); ++i)
            {
                theme.HueSlots[i] = Rgb(hues[i]);
            }

            theme.Deck.Kind = DeckKind::Gradient;
            theme.Deck.Color = Rgb(deckTop);
            theme.Deck.GradientEndColor = Rgb(deckFloor);

            theme.PlateColor = Rgb(plateTop);
            theme.PlateEndColor = Rgb(plateFloor);

            // The lift is the plate's own gradient, so a sheen would be saying it twice.
            theme.PlateSheenPercent = 0;

            // Nothing is raised on a tube. A screen has no thickness, so a drop shadow under a
            // raster box is the one thing that would give the illusion away.
            theme.PlateElevation = 0;
            theme.GlassTintPercent = 0;
            theme.FillAtRest = 0.0;
            theme.CornerRadius = 6;

            theme.BloomColor = Rgb(bloom);

            theme.Overlay.ScanLinePitch = 3;
            theme.Overlay.VignetteColor = Rgb(vignetteFloor);
            theme.Overlay.VignettePercent = 60;

            theme.Thumb = ThumbStyle::Neutral;
            theme.ValueFadesToLight = true;

            // A tube drives the raster harder under a finger rather than lifting a shadow.
            theme.TouchFillPercent = 19;

            return theme;
        }
    }

    std::vector<Theme> const& BuiltInThemes() noexcept
    {
        static std::vector<Theme> const themes = []
            {
                std::vector<Theme> list{};

                // ---- the original six ----
                //
                // Every dark theme sets its own track color. The design assumed they could all
                // leave it black; measuring says otherwise, because a near-black deck and a black
                // track are the same color to an eye across a room.

                {
                    auto studio = MakeDarkTheme(L"Studio Dark", 0x0C0D10,
                        { 0x4FC3F7, 0x81C784, 0xFFC247, 0xFF7043, 0xBA68C8, 0x4DD0E1 });

                    list.push_back(studio);
                }

                {
                    auto neon = MakeDarkTheme(L"Neon Booth", 0x08040F,
                        { 0x00E5FF, 0x76FF03, 0xFFEA00, 0xFF1744, 0xD500F9, 0x1DE9B6 });

                    list.push_back(neon);
                }

                {
                    // The one light theme in the original six, and the reason the track color
                    // had to become a property: black tracks on a near-white deck look like dirt.
                    auto daylight = MakeDarkTheme(L"Daylight", 0xF2F3F5,
                        { 0x0277BD, 0x2E7D32, 0xE65100, 0xC62828, 0x6A1B9A, 0x00695C });

                    daylight.TrackColor = Rgb(0xD5D7DB);
                    daylight.GlowStrength = 35;
                    daylight.GlassTintPercent = 92;
                    daylight.PlateSheenPercent = 0;
                    daylight.PlateElevation = 25;
                    daylight.ThumbColor = Rgb(0xFFFFFF);
                    daylight.ThumbEndColor = Rgb(0xD8DADE);

                    list.push_back(daylight);
                }

                {
                    // The P3 amber terminal. This is the swatch that has sat on the theme picker
                    // since the design sheet was drawn, finally built out of the tube it is
                    // named after rather than out of six oranges on a brown deck.
                    //
                    // Three things about that tube, and every one of them is a measurement
                    // rather than a preference. The glass is a warm maroon, not black. The halo
                    // is REDDER than the thing casting it, because P3 decays through red, and
                    // that one fact is what people recognize as the amber screen. And brightness
                    // moves hue, so the ramp runs like heat: ember, orange, amber, yellow, white
                    // hot. That last one gives this theme a second channel, which Cathode has
                    // none of: adjacent slots average delta E 21.9 against Cathode's 6.5, and
                    // the widest pair reaches 4.18 : 1 where Cathode managed 2.32.
                    //
                    // What it costs, measured and deliberately not engineered away: the bottom
                    // rung of the ramp is light, not letters. #C25316 as type measures 2.65 : 1
                    // on a lit plate and 4.10 : 1 inverted, and both fail what a label wants. It
                    // stays, because it is the color that makes the theme look real, and the
                    // rule that replaces the fudge is that the ember slot carries a rim, a value
                    // or a fill and never type.
                    auto amber = MakeTubeTheme(L"Amber Console",
                        0x33200F, 0x24160C, 0x140B07,
                        0x48301C, 0x3A2413,
                        0xFF6010,
                        { 0xFFB02E, 0xE0730F, 0xFFD76E, 0xC25316, 0xF59214, 0xFFF3D2 });

                    // Every recess on this theme is the palette floor at an alpha and nothing
                    // else. Hand-picked recess darks bottomed the blue channel out at zero once
                    // a scan line and the corner fall-off had both been laid over them, which is
                    // a pure black inside the one theme whose whole claim is that it has none.
                    amber.TrackColor = { 20, 11, 7, 204 };
                    amber.ArcTrackColor = { 0xFF, 0x60, 0x10, 51 };
                    amber.InkColor = Rgb(0xF0C489);

                    amber.Overlay.ScanLineColor = Rgb(0x120703);
                    amber.Overlay.ScanLineStrength = 44;

                    amber.RimStrengthPercent = 55;
                    amber.RestingGlowPercent = 16;
                    amber.GlowStrength = 60;
                    amber.PipeFalloff = 0.72;

                    // P3 holds on for a while, and a theme should be able to carry its own
                    // phosphor's persistence rather than making every binding say it.
                    amber.PersistenceMilliseconds = 400;

                    amber.ThumbColor = Rgb(0x6E4A28);
                    amber.ThumbEndColor = Rgb(0x402915);

                    amber.KeyWhiteColor = Rgb(0xF0C489);
                    amber.KeyBlackColor = Rgb(0x24160C);

                    // Amber, yellow, white hot reads as heat without anybody being told, and it
                    // survives with no color vision because it is also a rising brightness.
                    amber.MeterSlots = { 0, 2, 5 };

                    amber.CautionResourceKey = L"ThemeCautionAmberConsole";

                    list.push_back(amber);
                }

                {
                    auto blueprint = MakeDarkTheme(L"Blueprint", 0x0A1929,
                        { 0x90CAF9, 0x64B5F6, 0xE3F2FD, 0x42A5F5, 0xB3E5FC, 0x1E88E5 });

                    list.push_back(blueprint);
                }

                {
                    // Offered, never forced. If Windows switches to high contrast while a layout
                    // is running, the running layout is left alone and the offer waits until it
                    // is next opened, because reskinning a performer's surface mid set would be
                    // worse than the problem it solves.
                    // Magenta on black measures 6.7 : 1, which is under the bar this theme of all
                    // themes has to clear, so it is lightened rather than left at full saturation.
                    auto contrast = MakeDarkTheme(L"High contrast", 0x000000,
                        { 0x00FFFF, 0x00FF00, 0xFFFF00, 0xFF8080, 0xFF80FF, 0xFFFFFF });

                    contrast.GlassTintPercent = 0;
                    contrast.GlowStrength = 0;
                    contrast.CornerRadius = 0;
                    contrast.Labels = LabelPlacement::Below;
                    contrast.TrackColor = Rgb(0x767676);

                    // Nothing soft. Every edge on this theme is a hard line, because a blur is
                    // exactly what the person who turned high contrast on cannot resolve. So
                    // touch is a filled plate rather than a glow, and it is filled hard.
                    contrast.PlateSheenPercent = 0;
                    contrast.PlateElevation = 0;
                    contrast.PipeFalloff = 1.0;
                    contrast.Thumb = ThumbStyle::Hue;
                    contrast.TouchFillPercent = 55;

                    contrast.CautionResourceKey = L"ThemeCautionHighContrast";

                    list.push_back(contrast);
                }

                // ---- the tonal pair ----
                // Flat opaque plates instead of glass, with depth carried by a wash of the
                // control's own hue rather than a glow. This costs no new rendering layers,
                // which is the only reason it is worth doing.

                {
                    // The orange is darker than the dark themes use it. On a near-white deck the
                    // usual #EF6C00 measures 2.9 : 1, which a thin rim cannot carry.
                    auto pigment = MakeDarkTheme(L"Pigment Light", 0xFAF8F5,
                        { 0x1565C0, 0x2E7D32, 0xB35400, 0xC2185B, 0x6A1B9A, 0x00838F });

                    pigment.GlassTintPercent = 0;
                    pigment.GlowStrength = 0;
                    pigment.FillAtRest = 0.14;
                    pigment.TouchFillPercent = 27;
                    pigment.TrackColor = Rgb(0xE3DFD9);
                    pigment.CornerRadius = 14;

                    // Flat and tonal. Depth is the wash of the control's own color, so a sheen
                    // and a shadow would both be saying it a second time.
                    pigment.PlateSheenPercent = 0;
                    pigment.PlateElevation = 0;
                    pigment.PipeFalloff = 1.0;
                    pigment.Thumb = ThumbStyle::Hue;

                    pigment.CautionResourceKey = L"ThemeCautionPigment";

                    list.push_back(pigment);
                }

                {
                    auto pigment = MakeDarkTheme(L"Pigment Dark", 0x16151A,
                        { 0x64B5F6, 0x81C784, 0xFFB74D, 0xF06292, 0xBA68C8, 0x4DD0E1 });

                    pigment.GlassTintPercent = 0;
                    pigment.GlowStrength = 0;
                    pigment.FillAtRest = 0.18;
                    pigment.TouchFillPercent = 32;
                    pigment.CornerRadius = 14;

                    pigment.PlateSheenPercent = 0;
                    pigment.PlateElevation = 0;
                    pigment.PipeFalloff = 1.0;
                    pigment.Thumb = ThumbStyle::Hue;

                    pigment.CautionResourceKey = L"ThemeCautionPigment";

                    list.push_back(pigment);
                }

                // ---- Bigwig ----
                // Charcoal and gray, one lead orange, and a family of section colors. What makes
                // the product it plays on look machined is a ladder of grays rather than a color:
                // the page, one step up for a section, one more for a control, and a dark well cut
                // into the surface for anything that shows a value. Color only ever means the
                // value, the state, or which section this is, so at rest every control is the
                // same gray and a dense page stays readable.
                //
                // Every slot clears 3.0 on the page, a section and a control. Five of the six
                // clear 4.5 as a section name; red measures 3.1, so red is light, not letters.

                {
                    auto bigwig = MakeDarkTheme(L"Bigwig", 0x282828,
                        { 0xF98B31, 0x58B0F0, 0xE8C140, 0xC094F0, 0x6DBF4B, 0xEC5446 });

                    // The page is named at both ends rather than derived: the derived lift takes
                    // #282828 somewhere the product never goes.
                    bigwig.Deck.Color = Rgb(0x282828);
                    bigwig.Deck.GradientEndColor = Rgb(0x202020);

                    bigwig.GlassTintPercent = 0;
                    bigwig.GlowStrength = 0;
                    bigwig.FillAtRest = 0.0;
                    bigwig.TouchFillPercent = 26;

                    // A control: one step up from a section, a lit top and a dark edge.
                    bigwig.PlateColor = Rgb(0x474747);
                    bigwig.PlateEndColor = Rgb(0x3C3C3C);
                    bigwig.Rim = RimSource::NeutralEdge;
                    bigwig.NeutralRimColor = Rgb(0x1A1A1A);
                    bigwig.CornerRadius = 4;
                    bigwig.PlateSheenPercent = 3;
                    bigwig.PlateElevation = 40;

                    // No strip on a switch: lit is the color outright, so a strip would vanish
                    // into the fill, and at rest the product's buttons carry no color at all.
                    bigwig.ValueStrip = ValueStripPlacement::None;
                    bigwig.ValueIndicator = ValueIndicatorStyle::SolidArc;
                    bigwig.FillWhenOnPercent = 100;
                    bigwig.NamesInsideSwitches = true;

                    // A knob has no plate: a graded cap inside its arc, on the page or on a
                    // section alike.
                    bigwig.KnobFaceColor = Rgb(0x4A4A4A);
                    bigwig.KnobFaceEndColor = Rgb(0x1F1F1F);
                    bigwig.PointerColor = Rgb(0xE6E6E6);

                    // The fill is brightest at the value and falls away below it, and the cap is
                    // gray with a line of the control's color through it.
                    bigwig.PipeFalloff = 0.55;
                    bigwig.Thumb = ThumbStyle::Neutral;
                    bigwig.ThumbColor = Rgb(0x6E6E6E);
                    bigwig.ThumbEndColor = Rgb(0x4A4A4A);

                    // Cut into the surface: slots, fields and meters.
                    bigwig.TrackColor = Rgb(0x161616);
                    bigwig.WellColor = Rgb(0x161616);
                    bigwig.ArcTrackColor = { 0, 0, 0, 128 };
                    bigwig.InkColor = Rgb(0xC9C9C9);

                    // A section is one step up from the page and says its name in its color.
                    bigwig.PanelFill = PanelFillStyle::Color;
                    bigwig.PanelColor = Rgb(0x373737);
                    bigwig.PanelEndColor = Rgb(0x313131);
                    bigwig.SectionNameInHue = true;

                    bigwig.KeyWhiteColor = Rgb(0xD9D9D9);
                    bigwig.KeyBlackColor = Rgb(0x1F1F1F);

                    // Green, yellow and red like the desk it is a play on.
                    bigwig.MeterSlots = { 4, 2, 5 };

                    bigwig.CautionResourceKey = L"ThemeCautionBigwig";

                    list.push_back(bigwig);
                }

                // ---- Bone ----
                // Two colors: #E3DAC9 for the space and #8A795D for everything that has to read
                // as dark. Every other theme tells a control from its deck by VALUE - a dark
                // plate on a darker deck, or the reverse. This one cannot: a warm white plate
                // measures 1.23 : 1 against a bone deck. So the separation is carried entirely
                // by a soft warm shadow, and activity lights up white instead of the hue.
                //
                // Every color here was measured against both the plate and the deck at 3 : 1 or
                // better. The first ochre tried, #A67428, came out at 2.94 on bone and was
                // dropped for this one.

                {
                    auto bone = MakeDarkTheme(L"Bone", 0xE3DAC9,
                        { 0x8A795D, 0x5C6E4E, 0x8F6318, 0x9A543E, 0x5A647C, 0x9B3D34 });

                    // The deck is named rather than derived, because the derived floor shades
                    // BELOW bone, and the whole point of the theme is that the space is bone or
                    // a lifted bone and never anything else.
                    bone.Deck.Color = Rgb(0xF4EFE4);
                    bone.Deck.GradientEndColor = Rgb(0xE3DAC9);

                    bone.GlassTintPercent = 0;
                    bone.FillAtRest = 0.0;
                    bone.TouchFillPercent = 10;
                    bone.PlateColor = Rgb(0xFAF6EC);
                    bone.TrackColor = Rgb(0xD9D0BC);
                    bone.CornerRadius = 8;

                    // The rim carries the control's identity on its own here, so it runs close
                    // to full strength. At the 28 every other theme uses it measures 1.4 : 1
                    // against this plate and is simply not there.
                    bone.RimStrengthPercent = 85;

                    // The theme, in two lines. A white sheen on a near-white plate is only haze,
                    // so the light from above becomes the shadow underneath instead.
                    bone.PlateSheenPercent = 0;
                    bone.PlateElevation = 45;
                    bone.ShadowSpread = 9;
                    bone.ShadowColor = Rgb(0x5E5139);

                    bone.GlowStrength = 55;
                    bone.Light = LightSource::White;

                    // A white cap with a hairline of the control's color through it, and a flat
                    // value bar - a fade would be one more thing competing with the shadow.
                    bone.PipeFalloff = 1.0;
                    bone.Thumb = ThumbStyle::Neutral;
                    bone.ThumbColor = Rgb(0xFFFFFF);
                    bone.ThumbEndColor = Rgb(0xF0E9DB);

                    // The unfilled part of a knob's arc sits on bare deck rather than in the
                    // plate, so on this theme it has to be DARKER than the slot track, which is
                    // the mirror image of what the tube themes need.
                    bone.ArcTrackColor = { 138, 121, 93, 107 };

                    // A button carries its name, and lit it is the color outright, deepened a
                    // little so its warm white name still clears 4.5.
                    bone.NamesInsideSwitches = true;
                    bone.FillWhenOnPercent = 100;
                    bone.OnLiftPercent = -16;

                    // What holds a value is sunk and what your hand touches is raised. The slot
                    // and the pad's field carry a warm shadow inside, and the cap stands off the
                    // slot it rides.
                    bone.RecessShadePercent = 38;
                    bone.WellColor = Rgb(0xEDE6D8);
                    bone.ThumbShadowPercent = 34;

                    // Warm white naturals and the theme's one dark for the sharps.
                    bone.KeyWhiteColor = Rgb(0xFBF8F1);
                    bone.KeyBlackColor = Rgb(0x8A795D);

                    bone.MeterSlots = { 1, 2, 5 };

                    list.push_back(bone);
                }

                // ---- Cathode ----
                // An old black and white television, which is not black and white at all. The
                // glass is a dark sour green, the phosphor is BLUE-white, every edge is soft
                // because a beam has no hard edge, and there is no pure black or pure white
                // anywhere: the floor is #101611 and the ceiling is #F4F8FF, both on purpose.

                {
                    // Six slots, and all six are the same phosphor at six brightnesses. That is
                    // the theme, and it is also its one real cost: the widest pair in this
                    // palette measures 2.32 : 1 and the closest adjacent pair 1.10 : 1, so no
                    // two slots reach the 3 : 1 somebody needs to tell them apart. On Cathode a
                    // control is identified by where it sits and what it is called, never by its
                    // color. Nothing here is a fault to fix - a tube has one phosphor.
                    auto cathode = MakeTubeTheme(L"Cathode",
                        0x2C362C, 0x1B231E, 0x101611,
                        0x3A464C, 0x2C373C,
                        0xCFE0F3,
                        { 0xD2E1F3, 0xA9BDD2, 0xE4EEFB, 0x92A7BE, 0xBFD1E4, 0xF4F8FF });

                    cathode.TrackColor = { 13, 19, 15, 184 };
                    cathode.ArcTrackColor = { 0xCF, 0xE0, 0xF3, 38 };
                    cathode.InkColor = Rgb(0xCFDDEE);

                    cathode.Overlay.ScanLineColor = Rgb(0x060B08);
                    cathode.Overlay.ScanLineStrength = 46;

                    cathode.RimStrengthPercent = 52;
                    cathode.RestingGlowPercent = 13;
                    cathode.GlowStrength = 55;
                    cathode.PipeFalloff = 0.58;

                    cathode.ThumbColor = Rgb(0x4F5D64);
                    cathode.ThumbEndColor = Rgb(0x333F45);

                    // The phosphor for the naturals and the glass for the sharps. A tube has no
                    // white and no black, so a piano keyboard on one has to be drawn in the two
                    // colors it does have.
                    cathode.KeyWhiteColor = Rgb(0xCFDDEE);
                    cathode.KeyBlackColor = Rgb(0x1B231E);

                    // There is no green, amber and red to have, so the three zones are three
                    // brightnesses, and they take the widest three slots in the palette rather
                    // than three neighbors. It reads at all only because the segments touch: the
                    // eye is far better at ordering two levels side by side than at naming one.
                    cathode.MeterSlots = { 3, 0, 5 };

                    cathode.CautionResourceKey = L"ThemeCautionCathode";

                    list.push_back(cathode);
                }

                // ---- Terminal Green ----
                // The green screen, and the one that would have been got wrong from memory.
                // THE GLASS IS NOT GREEN. A terminal has a tinted anti-glare faceplate, so the
                // unlit screen is a cool blue-slate and the green sits a long way from it in
                // hue. Built as green on dark green it comes out as Cathode with the colors
                // swapped, and two of the three stop looking like different ideas.
                //
                // It needed no new property at all - it is the same set at different values,
                // which is the argument that this model is finished.

                {
                    auto green = MakeTubeTheme(L"Terminal Green",
                        0x1E262D, 0x161E24, 0x0B1013,
                        0x2A353D, 0x1E272E,
                        0x86F260,
                        { 0x4AE06A, 0x35B457, 0xA2F578, 0x2A944B, 0x3ECB60, 0xE4FFD6 });

                    green.TrackColor = { 11, 16, 19, 204 };
                    green.ArcTrackColor = { 0x86, 0xF2, 0x60, 41 };
                    green.InkColor = Rgb(0xA6DCB0);

                    green.Overlay.ScanLineColor = Rgb(0x04090B);
                    green.Overlay.ScanLineStrength = 46;

                    // The room, reflected in the glass. It is in every photograph of a real
                    // terminal and it is what makes glass read as glass rather than as paint.
                    green.Overlay.FaceplateSheenPercent = 5;
                    green.Overlay.FaceplateSheenColor = Rgb(0xBEE1F0);

                    green.RimStrengthPercent = 40;
                    green.RestingGlowPercent = 9;
                    green.GlowStrength = 38;
                    green.PipeFalloff = 0.22;

                    green.ThumbColor = Rgb(0x44545C);
                    green.ThumbEndColor = Rgb(0x232D34);

                    green.KeyWhiteColor = Rgb(0xA6DCB0);
                    green.KeyBlackColor = Rgb(0x161E24);

                    green.MeterSlots = { 0, 2, 5 };

                    green.CautionResourceKey = L"ThemeCautionTerminalGreen";

                    list.push_back(green);
                }

                // ---- Jove ----
                // Matte black steel, one orange, and a row of colored tabs. The first theme
                // where a control is not on a plate at all: a knob is a black cap sitting
                // straight on the panel and a fader is a slot milled into it.
                //
                // It inverts how every other theme uses the six slots. Every theme before this
                // spends a slot on a HAIRLINE; this one spends each of its six on a whole button
                // face, at full saturation, at rest - which is the exact thing the "nothing is
                // saturated at rest" rule exists to prevent. It gets away with it because the
                // panel around them is near-black and a panel has a few dozen tabs rather than a
                // few hundred, and the picker says so.

                {
                    // Every slot here is a whole button face with its own name printed on it,
                    // so each one has to carry type at 4.5 as well as clear the panel at 3.0.
                    // The comp's red measured 4.24 for type and is lifted until it clears.
                    auto jove = MakeDarkTheme(L"Jove", 0x262626,
                        { 0xE2483A, 0xE8892B, 0xE7D14A, 0x5CBE6A, 0x56C6CD, 0x4A7FC1 });

                    // Matte steel, not a lit deck. A front panel is painted, so it does not
                    // catch the light the way a sheet of glass does.
                    jove.Deck.Kind = DeckKind::SolidColor;
                    jove.Deck.Color = Rgb(0x262626);
                    jove.Deck.GradientEndColor = Rgb(0x262626);

                    // The cream tabs are the seventh color, and they are the absence of one.
                    jove.NeutralColor = Rgb(0xE8E2CE);

                    // A knob is a black cap on the steel; a tab is its own color, solid. One
                    // number for the whole theme cannot say both.
                    jove.PlateColor = Rgb(0x151515);
                    jove.PlateEndColor = Rgb(0x0C0C0C);

                    // A tab IS its color at rest, glossed: lighter at the top, darker at the
                    // bottom, with a light line along its molded top edge. Lit is the same
                    // plastic with the lamp behind it on - paler, and glowing in its own color.
                    jove.FillAtRest = 0.0;
                    jove.SwitchFillAtRest = 1.0;
                    jove.FillWhenOnPercent = 100;
                    jove.OnLiftPercent = 36;
                    jove.NamesInsideSwitches = true;
                    jove.GlassTintPercent = 0;
                    jove.PlateSheenPercent = 17;
                    jove.PlateShadePercent = 16;
                    jove.PlateHighlightPercent = 14;

                    // No rim and no light pipe. The control IS the part you touch.
                    jove.Rim = RimSource::None;
                    jove.ValueStrip = ValueStripPlacement::None;
                    jove.GlowStrength = 55;
                    jove.PlateElevation = 60;
                    jove.ShadowSpread = 5;
                    jove.PipeFalloff = 1.0;
                    jove.CornerRadius = 3;

                    // Every knob on this instrument is a black turned cap that points in the
                    // section orange, and every value is drawn in that orange too, whatever
                    // color the tabs around it are. The wide line across a fader cap is white.
                    jove.KnobFaceColor = Rgb(0x303030);
                    jove.KnobFaceEndColor = Rgb(0x090909);
                    jove.PointerColor = Rgb(0xE8601C);
                    jove.ValueColor = Rgb(0xE8601C);
                    jove.CapLineColor = Rgb(0xFFFFFF);
                    jove.CapLineWide = true;

                    // A fader is a slot milled straight into the steel: no plate at all, a cap
                    // standing well off it, and only a faint light in the slot below the cap,
                    // because on the instrument the cap position IS the value.
                    jove.FaderPlate = FaderPlateStyle::None;
                    jove.FaderFillPercent = 20;
                    jove.ThumbShadowPercent = 85;
                    jove.RecessShadePercent = 90;

                    jove.Thumb = ThumbStyle::Neutral;
                    jove.ThumbColor = Rgb(0x3C3C3C);
                    jove.ThumbEndColor = Rgb(0x0F0F0F);

                    // The section header is a filled orange banner over a section drawn as a
                    // faint outline rather than a box.
                    jove.SectionHeader = SectionHeaderStyle::FilledBar;
                    jove.PanelFill = PanelFillStyle::None;
                    jove.PanelOutlineColor = { 255, 255, 255, 33 };

                    // Silkscreen sits above what it names.
                    jove.Labels = LabelPlacement::Above;

                    jove.TrackColor = Rgb(0x080808);
                    jove.WellColor = Rgb(0x080808);
                    jove.ArcTrackColor = { 255, 255, 255, 26 };
                    jove.InkColor = Rgb(0xD8D4C8);

                    jove.KeyWhiteColor = Rgb(0xE8E2CE);
                    jove.KeyBlackColor = Rgb(0x151515);

                    jove.MeterSlots = { 3, 2, 0 };

                    jove.CautionResourceKey = L"ThemeCautionJove";

                    list.push_back(jove);
                }

                // ---- Supersaw ----
                // A matte blue panel with the texture of very fine sandpaper, shiny black
                // molding, and one small red lamp per switch.
                //
                // Two things here that no theme before it has asked for. The panel has a GRAIN -
                // every deck so far has been a color, a gradient or a photograph. And ON IS NOT
                // A PLATE STATE: a switch that is on looks exactly like a switch that is off,
                // except for three lit pixels above its label.

                {
                    // The comp's lamp red measured 2.94 : 1 against this panel, which is the
                    // same number that got an ochre rejected from Bone. Lifted until it clears.
                    auto saw = MakeDarkTheme(L"Supersaw", 0x39465F,
                        { 0xE8822A, 0xFF6047, 0x4CE57A, 0x96B7E2, 0xE2CE7E, 0xB09CE2 });

                    saw.Deck.Kind = DeckKind::SolidColor;
                    saw.Deck.Color = Rgb(0x39465F);
                    saw.Deck.GradientEndColor = Rgb(0x39465F);

                    // The grain is most of why the blue reads as a manufactured object rather
                    // than a fill.
                    saw.Overlay.GrainPercent = 34;
                    saw.Overlay.GrainColor = Rgb(0x8496B4);

                    // Shiny black molding, with a gloss line along its top edge.
                    saw.PlateColor = Rgb(0x1C1F25);
                    saw.PlateEndColor = Rgb(0x0D0F13);
                    saw.GlassTintPercent = 0;
                    saw.FillAtRest = 0.0;
                    saw.PlateSheenPercent = 9;
                    saw.PlateHighlightPercent = 24;
                    saw.PlateElevation = 46;
                    saw.ShadowSpread = 6;
                    saw.CornerRadius = 4;

                    // A switch says it is on with its lamp and leaves the plate black. Lit, the
                    // lamp puts a faint line of its own red round the switch. There is no value
                    // strip as well: a strip and a lamp is two lights saying one thing.
                    saw.FillWhenOnPercent = 0;
                    saw.LampColor = Rgb(0xFF523A);

                    saw.Rim = RimSource::None;
                    saw.ValueStrip = ValueStripPlacement::None;
                    saw.GlowStrength = 30;
                    saw.PipeFalloff = 1.0;

                    // A knob is a black turned body with a small light cap on its face, a white
                    // pointer, and a ring of short printed marks on the panel outside its arc.
                    saw.KnobFaceColor = Rgb(0x363B44);
                    saw.KnobFaceEndColor = Rgb(0x0D0F13);
                    saw.KnobCapColor = Rgb(0xD6DAE1);
                    saw.KnobCapEndColor = Rgb(0x9AA1AD);
                    saw.KnobCapSizePercent = 28;
                    saw.KnobTickCount = 9;
                    saw.NeutralColor = Rgb(0xC8CDD6);
                    saw.PointerColor = Rgb(0xE9ECF2);
                    saw.CapLineColor = Rgb(0xE9ECF2);

                    // A fader is a slot in a narrow strip of molding, with a cap wider than the
                    // strip standing off it and the slot below only partly lit.
                    saw.FaderPlate = FaderPlateStyle::Strip;
                    saw.FaderFillPercent = 34;
                    saw.ThumbShadowPercent = 80;
                    saw.RecessShadePercent = 85;

                    saw.Thumb = ThumbStyle::Neutral;
                    saw.ThumbColor = Rgb(0x2C3039);
                    saw.ThumbEndColor = Rgb(0x0C0E11);

                    // A section is a faint line on the panel with its name in orange, sitting in
                    // a gap cut into the top of the line.
                    saw.SectionHeader = SectionHeaderStyle::Notched;
                    saw.SectionNameInHue = true;
                    saw.PanelFill = PanelFillStyle::None;
                    saw.PanelOutlineColor = { 0xE9, 0xEC, 0xF2, 41 };

                    saw.Labels = LabelPlacement::Above;

                    saw.TrackColor = Rgb(0x090B0E);
                    saw.WellColor = Rgb(0x090B0E);
                    saw.ArcTrackColor = { 255, 255, 255, 26 };
                    saw.InkColor = Rgb(0xD4DCEA);

                    saw.KeyWhiteColor = Rgb(0xE9ECF2);
                    saw.KeyBlackColor = Rgb(0x141820);

                    saw.MeterSlots = { 2, 4, 1 };

                    saw.CautionResourceKey = L"ThemeCautionSupersaw";

                    list.push_back(saw);
                }

                // ---- Five-iSH ----
                // Black metal with the panel printed on it twice: a tan section named in black
                // across its top, and a green block inside it where the controls live, named in
                // white. Silver knobs with black caps, and the white line printed on the cap.
                //
                // Two surfaces and two inks on one page, which no theme before it has had. And
                // nothing on it is colored: every value is the print white, and the six slots are
                // the colors a panel lamp came in. They light a lamp or a meter and nothing else,
                // because no hue reads on the tan, the green and the black at once.

                {
                    auto five = MakeDarkTheme(L"Five-iSH", 0x1C1C1B,
                        { 0xFF4B36, 0xFFA43C, 0xF5D94C, 0x70DB6E, 0x86C4FF, 0xFFE2B0 });

                    five.Deck.Color = Rgb(0x222221);
                    five.Deck.GradientEndColor = Rgb(0x171716);

                    // The cream cap the instrument puts on the one row of sliders it wants you to
                    // find. A control on the neutral slot wears it; everything else is black.
                    five.NeutralColor = Rgb(0xE3DCC4);
                    five.NeutralCaps = true;

                    // Black molded plastic, with a gloss line along its top edge and a hard black
                    // edge all the way round.
                    five.PlateColor = Rgb(0x3A3A36);
                    five.PlateEndColor = Rgb(0x111110);
                    five.GlassTintPercent = 0;
                    five.FillAtRest = 0.0;
                    five.PlateSheenPercent = 0;
                    five.PlateHighlightPercent = 17;
                    five.PlateElevation = 60;
                    five.ShadowSpread = 3;
                    five.CornerRadius = 3;

                    five.Rim = RimSource::NeutralEdge;
                    five.NeutralRimColor = { 0, 0, 0, 217 };

                    // On lights a round lamp at the top of the cap and leaves the cap black.
                    five.FillWhenOnPercent = 0;
                    five.LampShape = LampStyle::Dot;
                    five.ValueStrip = ValueStripPlacement::None;
                    five.GlowStrength = 36;
                    five.PipeFalloff = 1.0;
                    five.TouchFillPercent = 14;

                    // Every value, every scale and every pointer is the print white.
                    five.ValueColor = Rgb(0xF2EFE3);
                    five.PointerColor = Rgb(0xF2EFE3);
                    five.CapLineColor = Rgb(0xF2EFE3);
                    five.CapLineWide = true;

                    // A silver skirt with a black cap on it. The line is on the cap, the part
                    // that turns, and a scale of eleven marks is printed round the outside.
                    five.KnobFaceColor = Rgb(0xF7F7F4);
                    five.KnobFaceEndColor = Rgb(0x6C6C67);
                    five.KnobCapColor = Rgb(0x3E3E3B);
                    five.KnobCapEndColor = Rgb(0x0A0A0A);
                    five.KnobCapSizePercent = 64;
                    five.KnobTickCount = 11;
                    five.PointerOnCap = true;
                    five.ArcTrackColor = { 8, 12, 8, 102 };

                    // A slider is a slot cut straight into the panel with a scale printed either
                    // side of it, a black cap with a white line, and only a faint light below the
                    // cap, because on the instrument the cap position is the whole value.
                    five.FaderPlate = FaderPlateStyle::None;
                    five.FaderFillPercent = 16;
                    five.FaderScalePercent = 62;
                    five.ThumbShadowPercent = 75;
                    five.RecessShadePercent = 90;

                    five.Thumb = ThumbStyle::Neutral;
                    five.ThumbColor = Rgb(0x4A4A45);
                    five.ThumbEndColor = Rgb(0x121211);

                    five.TrackColor = Rgb(0x121311);
                    five.WellColor = Rgb(0x121311);

                    // A section is a tan block printed flat on the metal and named in black across
                    // the middle of its top. A section inside it is the green.
                    five.PanelFill = PanelFillStyle::Color;
                    five.PanelColor = Rgb(0xCAC5AC);
                    five.PanelEndColor = Rgb(0xBFBA9F);
                    five.InsetPanelColor = Rgb(0x566D55);
                    five.InsetPanelEndColor = Rgb(0x4F654E);
                    five.PanelOutlineColor = { 0, 0, 0, 41 };
                    five.PanelElevation = 0;
                    five.SectionHeader = SectionHeaderStyle::Centered;

                    // White print on the green and the black, black print on the tan. Neither
                    // one reads on the other's surface.
                    five.InkColor = Rgb(0xF2EFE3);
                    five.SectionInkColor = Rgb(0x1D1D1A);

                    // The white rules printed on the metal between groups of sections.
                    five.RuleColor = { 242, 239, 227, 179 };
                    five.RuleFades = false;

                    five.Labels = LabelPlacement::Above;

                    five.KeyWhiteColor = Rgb(0xF2EFE3);
                    five.KeyBlackColor = Rgb(0x1D1D1A);

                    five.MeterSlots = { 3, 2, 0 };

                    five.CautionResourceKey = L"ThemeCautionFiveIsh";

                    list.push_back(five);
                }

                // ---- Airy System ----
                // Black brushed metal and green light. One panel with three families of control,
                // each lit a different way: what turns or slides is lit all the time, a button is
                // black until it is on, and a pad is colored plastic with a lamp behind it.
                //
                // Every theme before it lights all of its controls one way. This one needs three
                // resting states on one page.

                {
                    auto airy = MakeDarkTheme(L"Airy System", 0x131414,
                        { 0x35EE7A, 0xFF4D38, 0xFF9D3A, 0xEEE35A, 0x45A8FF, 0xB783FF });

                    airy.Deck.Color = Rgb(0x1A1B1B);
                    airy.Deck.GradientEndColor = Rgb(0x0C0D0D);

                    // The brushing: long fine streaks along the panel rather than specks, always
                    // lighter, the way the ridges of brushed metal catch the light.
                    airy.Overlay.GrainPercent = 32;
                    airy.Overlay.GrainColor = Rgb(0xFFFFFF);
                    airy.Overlay.GrainStreak = 48;

                    // The white steps on the drum machine: the absence of a color.
                    airy.NeutralColor = Rgb(0xE8ECEA);

                    airy.PlateColor = Rgb(0x232525);
                    airy.PlateEndColor = Rgb(0x0F1010);
                    airy.GlassTintPercent = 0;
                    airy.FillAtRest = 0.0;
                    airy.PlateSheenPercent = 0;
                    airy.PlateHighlightPercent = 11;
                    airy.PlateElevation = 60;
                    airy.ShadowSpread = 4;
                    airy.CornerRadius = 4;
                    airy.PipeFalloff = 1.0;

                    // A knob sits in a ring of its own light and a slider in a lit frame, touched
                    // or not. That light is how the panel says "this is a control" in a dark room.
                    airy.Rim = RimSource::ControlHue;
                    airy.RimStrengthPercent = 85;
                    airy.RestingGlowPercent = 30;
                    airy.GlowStrength = 70;
                    airy.TouchFillPercent = 20;

                    // A button is black until it is on, and then its edge lights in its own color.
                    // The comp fills a lit button at 12 per cent; the red one then measures 1.14 : 1
                    // against itself at rest, under the 1.15 every shipped theme's lit plate has
                    // to move by, so it is 14 here.
                    airy.SwitchRimStrengthPercent = 0;
                    airy.SwitchRestingGlowPercent = 0;
                    airy.FillWhenOnPercent = 14;
                    airy.ValueStrip = ValueStripPlacement::None;

                    // A pad is dim plastic at rest and its color outright when it is lit. The rest
                    // stops at a third: any stronger and the white and yellow pads land in the
                    // middle gray where neither a light name nor a dark one can be read.
                    airy.PadFillAtRest = 0.34;
                    airy.PadFillWhenOnPercent = 100;
                    airy.NamesInsideSwitches = true;

                    // A black cap in the ring. The ring is the control's own color at rest, and
                    // the value is the part of it lit full, with a halo.
                    airy.KnobFaceColor = Rgb(0x353737);
                    airy.KnobFaceEndColor = Rgb(0x090A0A);
                    airy.PointerColor = Rgb(0xEEF1EF);
                    airy.ArcTrackHuePercent = 30;
                    airy.ArcGlow = true;

                    // A slider is lit the way an AIRA one is: the frame and its glow go around the
                    // slot, the scale is printed outside them, and the cap is black with a white
                    // line. Crossing the lit frame is what lets a black cap read on a black panel.
                    airy.FaderPlate = FaderPlateStyle::Frame;
                    airy.FaderFillPercent = 38;
                    airy.FaderScalePercent = 45;
                    airy.ThumbShadowPercent = 80;
                    airy.RecessShadePercent = 90;

                    airy.Thumb = ThumbStyle::Neutral;
                    airy.ThumbColor = Rgb(0x353737);
                    airy.ThumbEndColor = Rgb(0x090A0A);
                    airy.CapLineColor = Rgb(0xFFFFFF);
                    airy.CapLineWide = true;

                    airy.TrackColor = Rgb(0x060707);
                    airy.WellColor = Rgb(0x060707);

                    // A section is a line of light with its name in the same light, centered
                    // across its top.
                    airy.PanelFill = PanelFillStyle::None;
                    airy.SectionHeader = SectionHeaderStyle::Centered;
                    airy.SectionNameInHue = true;

                    airy.RuleColor = { 228, 231, 229, 66 };
                    airy.RuleFades = false;

                    airy.Labels = LabelPlacement::Below;
                    airy.InkColor = Rgb(0xE4E7E5);

                    airy.KeyWhiteColor = Rgb(0xE4E7E5);
                    airy.KeyBlackColor = Rgb(0x131414);

                    airy.MeterSlots = { 0, 3, 1 };

                    airy.CautionResourceKey = L"ThemeCautionAirySystem";

                    list.push_back(airy);
                }

                return list;
            }();

        return themes;
    }

    _Use_decl_annotations_
    Theme const* FindBuiltInTheme(std::wstring const& name) noexcept
    {
        auto const& themes = BuiltInThemes();

        auto it = std::find_if(themes.begin(), themes.end(),
            [&name](Theme const& t) { return t.Name == name; });

        return it == themes.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    ThemeColor EffectiveArcTrackColor(Theme const& theme) noexcept
    {
        return theme.ArcTrackColor.A != 0 ? theme.ArcTrackColor : theme.TrackColor;
    }

    _Use_decl_annotations_
    ThemeColor EffectivePlateSheenColor(Theme const& theme) noexcept
    {
        return theme.PlateSheenColor.A != 0 ? theme.PlateSheenColor : ThemeColor{ 255, 255, 255, 255 };
    }

    _Use_decl_annotations_
    ThemeColor EffectiveVignetteColor(Theme const& theme) noexcept
    {
        if (theme.Overlay.VignetteColor.A != 0)
        {
            return theme.Overlay.VignetteColor;
        }

        // The deck's own floor taken further down. A vignette that stops at the floor is not a
        // vignette at all, because the floor is already what the bottom of the deck is.
        auto corner = ShadeBy(theme.Deck.GradientEndColor, 0.45);
        corner.A = 255;

        return corner;
    }

    _Use_decl_annotations_
    ThemeColor EffectiveFaceplateColor(Theme const& theme) noexcept
    {
        return theme.Overlay.FaceplateSheenColor.A != 0
            ? theme.Overlay.FaceplateSheenColor
            : ThemeColor{ 0xBE, 0xE1, 0xF0, 255 };
    }

    _Use_decl_annotations_
    ThemeColor EffectiveGrainColor(Theme const& theme) noexcept
    {
        if (theme.Overlay.GrainColor.A != 0)
        {
            return theme.Overlay.GrainColor;
        }

        // A speckle the deck could have cast itself. Derived rather than fixed so the grain
        // keeps the panel's own cast instead of turning a blue panel gray.
        auto lifted = theme.Deck.Color;

        lifted.R = static_cast<uint8_t>(std::clamp(lifted.R + 40, 0, 255));
        lifted.G = static_cast<uint8_t>(std::clamp(lifted.G + 40, 0, 255));
        lifted.B = static_cast<uint8_t>(std::clamp(lifted.B + 40, 0, 255));
        lifted.A = 255;

        return lifted;
    }

    // A plain white and black, which is what every keyboard was drawn with before a theme could
    // name its own. Kept as the fallback so a theme file written before this still draws the
    // same keyboard it always did.
    _Use_decl_annotations_
    ThemeColor EffectiveKeyWhiteColor(Theme const& theme) noexcept
    {
        return theme.KeyWhiteColor.A != 0 ? theme.KeyWhiteColor : ThemeColor{ 232, 234, 238, 255 };
    }

    _Use_decl_annotations_
    ThemeColor EffectiveKeyBlackColor(Theme const& theme) noexcept
    {
        return theme.KeyBlackColor.A != 0 ? theme.KeyBlackColor : ThemeColor{ 22, 25, 31, 255 };
    }

    _Use_decl_annotations_
    ThemeColor DeckColorAt(Theme const& theme, double fraction) noexcept
    {
        if (theme.Deck.Kind != DeckKind::Gradient)
        {
            return theme.Deck.Color;
        }

        auto const t = std::clamp(fraction, 0.0, 1.0);

        auto const mix = [t](uint8_t top, uint8_t bottom) noexcept
            {
                return static_cast<uint8_t>(std::clamp(
                    std::lround(top + (bottom - top) * t), 0L, 255L));
            };

        auto const& top = theme.Deck.Color;
        auto const& bottom = theme.Deck.GradientEndColor;

        return { mix(top.R, bottom.R), mix(top.G, bottom.G), mix(top.B, bottom.B), 255 };
    }

    _Use_decl_annotations_
    double FillAtRestFor(Theme const& theme, bool isSwitch) noexcept
    {
        return isSwitch && theme.SwitchFillAtRest >= 0.0
            ? theme.SwitchFillAtRest
            : theme.FillAtRest;
    }

    _Use_decl_annotations_
    bool HasNeutralColor(Theme const& theme) noexcept
    {
        return theme.NeutralColor.A != 0;
    }

    _Use_decl_annotations_
    int32_t EffectivePanelElevation(Theme const& theme) noexcept
    {
        return std::clamp(theme.PanelElevation >= 0 ? theme.PanelElevation : theme.PlateElevation, 0, 100);
    }

    _Use_decl_annotations_
    bool HasNamedBloomColor(Theme const& theme) noexcept
    {
        return theme.BloomColor.A != 0 || theme.Light == LightSource::White;
    }

    _Use_decl_annotations_
    ThemeColor NamedBloomColor(Theme const& theme) noexcept
    {
        if (theme.BloomColor.A != 0)
        {
            auto named = theme.BloomColor;
            named.A = 255;

            return named;
        }

        return { 255, 255, 255, 255 };
    }

    _Use_decl_annotations_
    double RelativeLuminance(ThemeColor const& color) noexcept
    {
        auto const channel = [](uint8_t value) noexcept
            {
                auto const normalized = value / 255.0;

                return normalized <= 0.03928
                    ? normalized / 12.92
                    : std::pow((normalized + 0.055) / 1.055, 2.4);
            };

        return 0.2126 * channel(color.R) + 0.7152 * channel(color.G) + 0.0722 * channel(color.B);
    }

    _Use_decl_annotations_
    double ContrastRatio(ThemeColor const& first, ThemeColor const& second) noexcept
    {
        auto const a = RelativeLuminance(first);
        auto const b = RelativeLuminance(second);

        auto const lighter = (std::max)(a, b);
        auto const darker = (std::min)(a, b);

        return (lighter + 0.05) / (darker + 0.05);
    }

    _Use_decl_annotations_
    std::vector<SlotContrast> MeasureContrast(Theme const& theme) noexcept
    {
        std::vector<SlotContrast> results{};
        results.reserve(ThemeHueSlotCount);

        for (int32_t i = 0; i < ThemeHueSlotCount; ++i)
        {
            SlotContrast slot{};

            slot.SlotIndex = i;
            slot.Ratio = ContrastRatio(theme.HueSlots[static_cast<size_t>(i)], theme.Deck.Color);
            slot.MeetsMinimum = slot.Ratio >= MinimumSlotContrast;

            results.push_back(slot);
        }

        return results;
    }
}
