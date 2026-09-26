// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThemeTests.h"

#include <algorithm>

#include "ThemeModel.h"
#include "SurfaceColors.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

void ThemeTests::ShipsTheTenThemesTheDesignNames()
{
    auto const& themes = glass::BuiltInThemes();

    VERIFY_ARE_EQUAL(size_t{ 10 }, themes.size());

    wchar_t const* expected[]
    {
        L"Studio Dark", L"Neon Booth", L"Daylight", L"Amber Console", L"Blueprint",
        L"High contrast", L"Pigment Light", L"Pigment Dark", L"Bigwig", L"Bone",
    };

    for (auto const* name : expected)
    {
        VERIFY_IS_NOT_NULL(glass::FindBuiltInTheme(name));
    }

    // Studio Dark is the default and the one that stays readable on the densest page, so it is
    // first rather than alphabetical.
    VERIFY_ARE_EQUAL(std::wstring{ L"Studio Dark" }, themes[0].Name);

    VERIFY_IS_NULL(glass::FindBuiltInTheme(L"Not A Theme"));
}

void ThemeTests::EveryBuiltInThemeFillsAllSixSlots()
{
    for (auto const& theme : glass::BuiltInThemes())
    {
        VERIFY_IS_TRUE(theme.IsBuiltIn);
        VERIFY_IS_FALSE(theme.Name.empty());

        for (int32_t i = 0; i < glass::ThemeHueSlotCount; ++i)
        {
            auto const& slot = theme.HueSlots[static_cast<size_t>(i)];

            // A slot left at zero is a control that disappears. Easy to do, and invisible until
            // somebody picks that slot on stage.
            VERIFY_ARE_EQUAL(uint8_t{ 255 }, slot.A);
            VERIFY_IS_TRUE(slot.R != 0 || slot.G != 0 || slot.B != 0);
        }
    }
}

void ThemeTests::ContrastMatchesTheWcagReferenceValues()
{
    constexpr glass::ThemeColor white{ 255, 255, 255, 255 };
    constexpr glass::ThemeColor black{ 0, 0, 0, 255 };

    // The two ends of the scale are defined by the standard, so they are the check that the
    // formula is the real one rather than something that merely returns a plausible number.
    VERIFY_IS_LESS_THAN(std::fabs(glass::ContrastRatio(white, black) - 21.0), 0.01);
    VERIFY_IS_LESS_THAN(std::fabs(glass::ContrastRatio(white, white) - 1.0), 0.01);

    VERIFY_IS_LESS_THAN(std::fabs(glass::RelativeLuminance(white) - 1.0), 0.001);
    VERIFY_IS_LESS_THAN(std::fabs(glass::RelativeLuminance(black) - 0.0), 0.001);

    // Mid grey against white, a published worked example.
    constexpr glass::ThemeColor midGrey{ 119, 119, 119, 255 };
    auto const ratio = glass::ContrastRatio(midGrey, white);

    Log::Comment(String().Format(L"#777777 on white measures %.2f : 1", ratio));
    VERIFY_IS_LESS_THAN(std::fabs(ratio - 4.48), 0.05);
}

void ThemeTests::MeasuresEverySlotAgainstTheDeck()
{
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);

    auto const measured = glass::MeasureContrast(*studio);

    VERIFY_ARE_EQUAL(size_t{ glass::ThemeHueSlotCount }, measured.size());

    for (int32_t i = 0; i < glass::ThemeHueSlotCount; ++i)
    {
        VERIFY_ARE_EQUAL(i, measured[static_cast<size_t>(i)].SlotIndex);
        VERIFY_IS_GREATER_THAN(measured[static_cast<size_t>(i)].Ratio, 1.0);
    }
}

void ThemeTests::EverySlotOfEveryShippedThemeIsLegible()
{
    // Contrast is measured, not guessed. A theme we ship with an illegible slot is worse than one
    // a customer built badly, because they will assume ours is the reference.
    for (auto const& theme : glass::BuiltInThemes())
    {
        for (auto const& slot : glass::MeasureContrast(theme))
        {
            Log::Comment(String().Format(L"%s slot %d measures %.2f : 1",
                theme.Name.c_str(), slot.SlotIndex, slot.Ratio));

            if (!slot.MeetsMinimum)
            {
                Log::Error(String().Format(
                    L"%s slot %d is %.2f : 1, below the %.1f : 1 a thin rim needs.",
                    theme.Name.c_str(), slot.SlotIndex, slot.Ratio, glass::MinimumSlotContrast));
            }

            VERIFY_IS_TRUE(slot.MeetsMinimum);
        }
    }
}

void ThemeTests::NoThemeLeavesTheTrackColorAgainstItsOwnDeck()
{
    // The dark themes get away with a black track. A light theme does not, and this is the gap
    // that would have bitten the first customer who built one.
    for (auto const& theme : glass::BuiltInThemes())
    {
        auto const ratio = glass::ContrastRatio(theme.TrackColor, theme.Deck.Color);

        Log::Comment(String().Format(L"%s track on deck measures %.2f : 1", theme.Name.c_str(), ratio));

        // The track is the empty part of a slot, so it only has to be visible as a groove, not
        // readable as text. What it must not be is identical to the deck.
        VERIFY_IS_GREATER_THAN(ratio, 1.05);
    }
}

void ThemeTests::BigwigUsesANeutralRimAndOneHue()
{
    auto const* bigwig = glass::FindBuiltInTheme(L"Bigwig");
    VERIFY_IS_NOT_NULL(bigwig);

    VERIFY_IS_TRUE(bigwig->Rim == glass::RimSource::NeutralEdge);
    VERIFY_IS_TRUE(bigwig->ValueStrip == glass::ValueStripPlacement::Top);
    VERIFY_IS_TRUE(bigwig->ValueIndicator == glass::ValueIndicatorStyle::SegmentedLamps);

    // Below this the lamps stop separating and the ring reads as a fine comb, so a knob that
    // small falls back to the solid arc on its own.
    VERIFY_IS_GREATER_THAN(bigwig->MinimumLampRingSize, 0);
    VERIFY_IS_GREATER_THAN(bigwig->LampCount, 0);

    // One strong orange doing all the work. That restraint is the whole theme.
    for (int32_t i = 1; i < glass::ThemeHueSlotCount; ++i)
    {
        VERIFY_IS_TRUE(bigwig->HueSlots[0] == bigwig->HueSlots[static_cast<size_t>(i)]);
    }
}

void ThemeTests::TheTonalThemesTurnOffTheGlass()
{
    for (auto const* name : { L"Pigment Light", L"Pigment Dark" })
    {
        auto const* theme = glass::FindBuiltInTheme(name);
        VERIFY_IS_NOT_NULL(theme);

        // Flat opaque plates with the depth carried by a wash of the control's own hue. If the
        // glass or the glow were left on, these would just be a recolor.
        VERIFY_ARE_EQUAL(0, theme->GlassTintPercent);
        VERIFY_ARE_EQUAL(0, theme->GlowStrength);
        VERIFY_IS_GREATER_THAN(theme->FillAtRest, 0.0);
        VERIFY_IS_GREATER_THAN(theme->CornerRadius, 7);
    }

    // Bigwig uses the same machinery with the wash turned off, which is what keeps its plates
    // neutral grey.
    auto const* bigwig = glass::FindBuiltInTheme(L"Bigwig");
    VERIFY_IS_NOT_NULL(bigwig);
    VERIFY_ARE_EQUAL(0.0, bigwig->FillAtRest);
}

void ThemeTests::HighContrastTurnsOffEveryEffect()
{
    auto const* theme = glass::FindBuiltInTheme(L"High contrast");
    VERIFY_IS_NOT_NULL(theme);

    VERIFY_ARE_EQUAL(0, theme->GlassTintPercent);
    VERIFY_ARE_EQUAL(0, theme->GlowStrength);
    VERIFY_ARE_EQUAL(0, theme->CornerRadius);

    // and it has to clear the bar by a wide margin, not scrape it
    for (auto const& slot : glass::MeasureContrast(*theme))
    {
        VERIFY_IS_GREATER_THAN(slot.Ratio, 7.0);
    }
}

void ThemeTests::BoneIsSeparatedByItsShadowRatherThanItsValue()
{
    auto const* bone = glass::FindBuiltInTheme(L"Bone");
    VERIFY_IS_NOT_NULL(bone);

    // The claim the whole theme rests on, measured rather than asserted: there is essentially no
    // value difference between a Bone plate and a Bone deck. Every other theme has one, and that
    // is what it uses to make a control read as an object.
    auto const platedness = glass::ContrastRatio(bone->PlateColor, bone->Deck.GradientEndColor);

    Log::Comment(String().Format(L"Bone plate on deck measures %.2f : 1", platedness));
    VERIFY_IS_LESS_THAN(platedness, 1.5);

    // So the shadow is structural. Turning it off does not give a flatter theme, it gives a
    // blank sheet, which is why a theme editor should not offer zero here.
    VERIFY_IS_GREATER_THAN(bone->PlateElevation, 0);

    // And it has to be warm. A black shadow on a bone deck comes out a dirty gray.
    VERIFY_IS_TRUE(bone->ShadowColor.R > bone->ShadowColor.B);
    VERIFY_IS_GREATER_THAN(static_cast<int32_t>(bone->ShadowColor.R), 0);

    // Activity lights up white, because the plate is already near-white and a hue has nowhere
    // to glow to.
    VERIFY_IS_TRUE(bone->Light == glass::LightSource::White);
    VERIFY_IS_GREATER_THAN(bone->GlowStrength, 0);

    glass::Control control{};
    control.HueSlot = 0;

    auto const colors = glass::ResolveControlColors(control, *bone);

    VERIFY_ARE_EQUAL(uint8_t{ 255 }, colors.Bloom.R);
    VERIFY_ARE_EQUAL(uint8_t{ 255 }, colors.Bloom.G);
    VERIFY_ARE_EQUAL(uint8_t{ 255 }, colors.Bloom.B);
    VERIFY_IS_GREATER_THAN(static_cast<int32_t>(colors.Bloom.A), 0);
}

void ThemeTests::BoneNeverLetsTheSpaceGoDarkerThanBone()
{
    auto const* bone = glass::FindBuiltInTheme(L"Bone");
    VERIFY_IS_NOT_NULL(bone);

    // The deck is named rather than derived on this one theme, and this is why: the derived
    // floor shades below the color the theme is named for. The space is bone or a lifted bone
    // and never anything else, so the darker end of the gradient IS bone.
    constexpr glass::ThemeColor named{ 0xE3, 0xDA, 0xC9, 255 };

    VERIFY_IS_TRUE(bone->Deck.GradientEndColor == named);
    VERIFY_IS_GREATER_THAN(
        glass::RelativeLuminance(bone->Deck.Color),
        glass::RelativeLuminance(bone->Deck.GradientEndColor));
}

void ThemeTests::OnlyALightThemeRaisesItsRestingRim()
{
    // Nothing is saturated at rest, and the six dark themes keep the quarter-strength rim that
    // rule produces. This is the guard on that: a light theme needing a stronger hairline must
    // not drag the dark ones up with it.
    for (auto const* name : { L"Studio Dark", L"Neon Booth", L"Amber Console", L"Blueprint",
        L"High contrast", L"Pigment Dark" })
    {
        auto const* theme = glass::FindBuiltInTheme(name);
        VERIFY_IS_NOT_NULL(theme);
        VERIFY_ARE_EQUAL(28, theme->RimStrengthPercent);
    }

    auto const* bone = glass::FindBuiltInTheme(L"Bone");
    VERIFY_IS_NOT_NULL(bone);

    // A hairline that reads as a line on near-black is simply not there on near-white.
    VERIFY_IS_GREATER_THAN(bone->RimStrengthPercent, 50);

    glass::Control control{};
    control.HueSlot = 0;

    auto const dark = glass::ResolveControlColors(control, *glass::FindBuiltInTheme(L"Studio Dark"));
    auto const light = glass::ResolveControlColors(control, *bone);

    VERIFY_IS_GREATER_THAN(static_cast<int32_t>(light.Rim.A), static_cast<int32_t>(dark.Rim.A));
}

void ThemeTests::OnlyBoneMovesTheShadowOffItsShippedGeometry()
{
    // The renderer used to hardcode a 3 pixel blur and a 1 pixel drop. Spread is now a theme
    // number, and the offset is a third of it, so a spread of 3 reproduces those two constants
    // exactly. This is the guard that says so: every theme drawn before Bone still has it.
    for (auto const& theme : glass::BuiltInThemes())
    {
        if (theme.Name == L"Bone")
        {
            continue;
        }

        VERIFY_ARE_EQUAL(3, theme.ShadowSpread);

        // and a black shadow, which is what every one of them was drawn with
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.ShadowColor.R);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.ShadowColor.G);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.ShadowColor.B);

        VERIFY_IS_TRUE(theme.Light == glass::LightSource::ControlHue);
    }

    // Measured on screen at a spread of 3: a white plate on a bone deck darkened the single row
    // of pixels under it by four values and then stopped. That is not a shadow, and on this one
    // theme the shadow is the only thing there is.
    auto const* bone = glass::FindBuiltInTheme(L"Bone");
    VERIFY_IS_NOT_NULL(bone);
    VERIFY_IS_GREATER_THAN(bone->ShadowSpread, 3);
}
