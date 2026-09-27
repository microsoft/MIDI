// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThemeTests.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>

#include "ThemeModel.h"
#include "SurfaceColors.h"
#include "PadGrid.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

void ThemeTests::ShipsTheThemesTheDesignNames()
{
    auto const& themes = glass::BuiltInThemes();

    VERIFY_ARE_EQUAL(size_t{ 16 }, themes.size());

    wchar_t const* expected[]
    {
        L"Studio Dark", L"Neon Booth", L"Daylight", L"Terminal Amber", L"Blueprint",
        L"High contrast", L"Tonal Light", L"Tonal Dark", L"Bigwig", L"Bone",
        L"Cathode", L"Terminal Green", L"Jove", L"Supersaw", L"Five-iSH", L"Airy System",
    };

    for (auto const* name : expected)
    {
        VERIFY_IS_NOT_NULL(glass::FindBuiltInTheme(name));
    }

    // Studio Dark is the default and the one that stays readable on the densest page, so it is
    // first rather than alphabetical.
    VERIFY_ARE_EQUAL(std::wstring{ L"Studio Dark" }, themes[0].Name);

    // After it, alphabetical, so a family of themes sits together in the picker.
    for (size_t index = 2; index < themes.size(); ++index)
    {
        VERIFY_IS_TRUE(_wcsicmp(themes[index - 1].Name.c_str(), themes[index].Name.c_str()) < 0);
    }

    VERIFY_IS_NULL(glass::FindBuiltInTheme(L"Not A Theme"));

    // A layout saved before a theme was renamed still finds it.
    VERIFY_ARE_EQUAL(std::wstring{ L"Tonal Light" }, glass::FindBuiltInTheme(L"Pigment Light")->Name);
    VERIFY_ARE_EQUAL(std::wstring{ L"Tonal Dark" }, glass::FindBuiltInTheme(L"Pigment Dark")->Name);
    VERIFY_ARE_EQUAL(std::wstring{ L"Terminal Amber" }, glass::FindBuiltInTheme(L"Amber Console")->Name);
    VERIFY_ARE_EQUAL(std::wstring{ L"Bone" }, glass::CurrentThemeName(L"Bone"));
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

void ThemeTests::BigwigIsALadderOfGraysWithColorOnlyForTheValue()
{
    auto const* bigwig = glass::FindBuiltInTheme(L"Bigwig");
    VERIFY_IS_NOT_NULL(bigwig);

    // A neutral edge, so color only ever means the value, the state, or which section this is.
    VERIFY_IS_TRUE(bigwig->Rim == glass::RimSource::NeutralEdge);

    // No value strip: a lit switch is its color outright, and a strip would vanish into it.
    VERIFY_IS_TRUE(bigwig->ValueStrip == glass::ValueStripPlacement::None);
    VERIFY_ARE_EQUAL(100, bigwig->FillWhenOnPercent);
    VERIFY_IS_TRUE(bigwig->NamesInsideSwitches);

    // What makes the product look machined is a ladder of grays rather than a color: a well cut
    // below the page, the page, a section one step up, and a control one more.
    VERIFY_IS_TRUE(bigwig->PanelFill == glass::PanelFillStyle::Color);

    auto const well = glass::RelativeLuminance(bigwig->WellColor);
    auto const page = glass::RelativeLuminance(bigwig->Deck.Color);
    auto const section = glass::RelativeLuminance(bigwig->PanelColor);
    auto const control = glass::RelativeLuminance(bigwig->PlateColor);

    Log::Comment(String().Format(L"Bigwig well %.4f, page %.4f, section %.4f, control %.4f",
        well, page, section, control));

    VERIFY_IS_LESS_THAN(well, page);
    VERIFY_IS_LESS_THAN(page, section);
    VERIFY_IS_LESS_THAN(section, control);

    // A value sits on the page, on a section or in a well, so every slot has to clear 3.0 on the
    // two it can sit on that are not already near black.
    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        auto const& color = bigwig->HueSlots[static_cast<size_t>(slot)];

        auto const onPage = glass::ContrastRatio(color, bigwig->Deck.Color);
        auto const onSection = glass::ContrastRatio(color, bigwig->PanelColor);

        Log::Comment(String().Format(L"Bigwig slot %d: %.2f on the page, %.2f on a section",
            slot + 1, onPage, onSection));

        VERIFY_IS_GREATER_THAN_OR_EQUAL(onPage, glass::MinimumSlotContrast);
        VERIFY_IS_GREATER_THAN_OR_EQUAL(onSection, glass::MinimumSlotContrast);
    }

    // Lit is the hue top to bottom, and the name on it turns dark so it still reads.
    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;
    tab.HueSlot = 0;

    auto const colors = glass::ResolveControlColors(tab, *bigwig);

    VERIFY_IS_TRUE((colors.OnPlate == bigwig->HueSlots[0]));
    VERIFY_IS_TRUE((colors.OnPlateEnd == bigwig->HueSlots[0]));
    VERIFY_IS_GREATER_THAN_OR_EQUAL(glass::ContrastRatio(colors.SwitchInkOn, colors.OnPlate), 4.5);
    VERIFY_IS_LESS_THAN(glass::RelativeLuminance(colors.SwitchInkOn), glass::RelativeLuminance(colors.OnPlate));

    // A knob has no plate of its own: a graded cap, lit above the middle.
    VERIFY_IS_TRUE(bigwig->KnobFaceColor.A != 0);
    VERIFY_IS_GREATER_THAN(
        glass::RelativeLuminance(bigwig->KnobFaceColor),
        glass::RelativeLuminance(bigwig->KnobFaceEndColor));
}

void ThemeTests::TheTonalThemesTurnOffTheGlass()
{
    for (auto const* name : { L"Tonal Light", L"Tonal Dark" })
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

    // Every theme without a glow has to say touch some other way, or a finger on a control does
    // nothing visible at all. On the flat ones the plate is the only thing left.
    for (auto const& theme : glass::BuiltInThemes())
    {
        if (theme.GlowStrength > 0 || theme.RestingGlowPercent > 0)
        {
            continue;
        }

        Log::Comment(String().Format(L"%s has no glow, so its touch fill is %d per cent",
            theme.Name.c_str(), theme.TouchFillPercent));

        VERIFY_IS_GREATER_THAN(theme.TouchFillPercent, 0);

        glass::Control control{};
        control.HueSlot = 0;

        auto const colors = glass::ResolveControlColors(control, theme);

        // And it has to land somewhere different from the resting plate, or it says nothing.
        VERIFY_IS_TRUE(colors.TouchPlate.A != 0);
        VERIFY_IS_FALSE((colors.TouchPlate == colors.Plate));
    }

    // !! A ThemeColor defaults its ALPHA to 255, so a plain ThemeColor member is opaque BLACK
    // rather than nothing. That one default turned "this theme has no touch fill" into a black
    // disc painted over every control the moment it was pressed, on four shipped themes, and it
    // took a real pointer and a pixel read to find. Any theme that does not ask for a touch fill
    // has to resolve to nothing at all.
    {
        auto theme = *glass::FindBuiltInTheme(L"Studio Dark");
        theme.TouchFillPercent = 0;

        glass::Control control{};
        control.HueSlot = 0;

        auto const colors = glass::ResolveControlColors(control, theme);

        VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.TouchPlate.A);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.TouchPlateEnd.A);
    }
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
    // Nothing is saturated at rest, and the plain dark themes keep the quarter-strength rim that
    // rule produces. This is the guard on that: a theme needing a stronger hairline must not
    // drag the others up with it.
    for (auto const* name : { L"Studio Dark", L"Neon Booth", L"Blueprint",
        L"High contrast", L"Tonal Dark" })
    {
        auto const* theme = glass::FindBuiltInTheme(name);
        VERIFY_IS_NOT_NULL(theme);
        VERIFY_ARE_EQUAL(28, theme->RimStrengthPercent);
    }

    auto const* bone = glass::FindBuiltInTheme(L"Bone");
    VERIFY_IS_NOT_NULL(bone);

    // A hairline that reads as a line on near-black is simply not there on near-white.
    VERIFY_IS_GREATER_THAN(bone->RimStrengthPercent, 50);

    // A tube sits between the two: the rim is one of only two things separating a raster box
    // from the glass, and the other one is the spill.
    for (auto const* name : { L"Cathode", L"Terminal Amber", L"Terminal Green" })
    {
        auto const* theme = glass::FindBuiltInTheme(name);
        VERIFY_IS_NOT_NULL(theme);

        VERIFY_IS_GREATER_THAN(theme->RimStrengthPercent, 28);
        VERIFY_IS_LESS_THAN(theme->RimStrengthPercent, 85);
    }

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
    //
    // The two hardware panels are allowed off it as well, and for the same reason Bone is: a
    // knob bolted to a steel panel casts a real shadow on it, and that shadow is part of what
    // makes the control read as an object sitting on a surface rather than as paint. Airy
    // System's controls stand off brushed metal the same way.
    wchar_t const* const raised[]{ L"Bone", L"Jove", L"Supersaw", L"Airy System" };

    auto const isRaised = [&raised](std::wstring const& name)
        {
            for (auto const* one : raised)
            {
                if (name == one)
                {
                    return true;
                }
            }

            return false;
        };

    for (auto const& theme : glass::BuiltInThemes())
    {
        if (isRaised(theme.Name))
        {
            // Whatever it moved to, it moved DELIBERATELY - a spread of 3 here would mean
            // somebody reset it and did not notice.
            VERIFY_IS_GREATER_THAN(theme.ShadowSpread, 3);
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

// ============================================================================
// The three cathode ray tube themes, and the engine work they earned.
// ============================================================================

namespace
{
    wchar_t const* const TubeThemeNames[]{ L"Cathode", L"Terminal Amber", L"Terminal Green" };

    bool IsTubeTheme(_In_ glass::Theme const& theme) noexcept
    {
        for (auto const* name : TubeThemeNames)
        {
            if (theme.Name == name)
            {
                return true;
            }
        }

        return false;
    }
}

void ThemeTests::OnlyATubeThemeLaysAnOverlayOverTheDeck()
{
    // The overlay is the one genuinely new rendering element in the whole set, and it is one
    // visual for the whole page. This is the guard that says nothing else grew one by accident:
    // a scan line on Studio Dark would be a defect nobody would think to look for.
    //
    // Grain lives in the same overlay but is not a tube thing - it is the texture of a painted
    // panel - so it is checked separately below rather than treated as a scan line.
    for (auto const& theme : glass::BuiltInThemes())
    {
        if (!IsTubeTheme(theme))
        {
            VERIFY_ARE_EQUAL(0, theme.Overlay.ScanLinePitch);
            VERIFY_ARE_EQUAL(0, theme.Overlay.VignettePercent);
            VERIFY_ARE_EQUAL(0, theme.Overlay.FaceplateSheenPercent);
            continue;
        }

        VERIFY_IS_FALSE(theme.Overlay.IsEmpty());

        // A tube is not a painted panel, so it has no grain.
        VERIFY_ARE_EQUAL(0, theme.Overlay.GrainPercent);

        // One dark line every three screen pixels, which is what the comps draw.
        VERIFY_ARE_EQUAL(3, theme.Overlay.ScanLinePitch);
        VERIFY_IS_GREATER_THAN(theme.Overlay.ScanLineStrength, 0);
        VERIFY_IS_GREATER_THAN(theme.Overlay.VignettePercent, 0);
    }

    // Only the terminal reflects the room. A television and a console do not, and putting the
    // highlight on all three would make them look like one theme recolored.
    auto const* green = glass::FindBuiltInTheme(L"Terminal Green");
    VERIFY_IS_NOT_NULL(green);
    VERIFY_IS_GREATER_THAN(green->Overlay.FaceplateSheenPercent, 0);

    for (auto const* name : { L"Cathode", L"Terminal Amber" })
    {
        VERIFY_ARE_EQUAL(0, glass::FindBuiltInTheme(name)->Overlay.FaceplateSheenPercent);
    }
}

void ThemeTests::EveryTubeThemeCarriesItsControlsOnLightRatherThanValue()
{
    // The claim the three of them rest on, measured rather than asserted: a raster box has
    // essentially no brightness difference from the glass it is drawn on. Every other theme
    // tells a control from its deck by value, and these cannot.
    for (auto const* name : TubeThemeNames)
    {
        auto const* theme = glass::FindBuiltInTheme(name);
        VERIFY_IS_NOT_NULL(theme);

        auto const platedness = glass::ContrastRatio(theme->PlateColor, theme->Deck.Color);

        Log::Comment(String().Format(L"%s plate on deck measures %.2f : 1", name, platedness));
        VERIFY_IS_LESS_THAN(platedness, 1.5);

        // So the spill is the structure, and a theme editor has to refuse to let it reach zero.
        VERIFY_IS_GREATER_THAN(theme->RestingGlowPercent, 0);

        // A screen has no thickness. A drop shadow under a raster box is the one thing that
        // would give the illusion away.
        VERIFY_ARE_EQUAL(0, theme->PlateElevation);

        // The lift is the plate's own gradient, so a sheen would be saying it twice.
        VERIFY_ARE_EQUAL(0, theme->PlateSheenPercent);
        VERIFY_IS_TRUE(theme->PlateEndColor.A != 0);

        // The ink is the phosphor rather than a neutral white measured off the glass, and it
        // still has to be readable on the plate it lands on.
        VERIFY_IS_TRUE(theme->InkColor.A != 0);

        glass::Control control{};
        control.HueSlot = 0;

        auto const colors = glass::ResolveControlColors(control, *theme);

        VERIFY_IS_TRUE((colors.Label == theme->InkColor));

        auto const readability = glass::ContrastRatio(theme->InkColor, theme->PlateColor);

        Log::Comment(String().Format(L"%s ink on its plate measures %.2f : 1", name, readability));
        VERIFY_IS_GREATER_THAN(readability, 4.5);
    }

    // And nothing else picked up a resting glow on the way past. Airy System is the one panel
    // that asked for it: a knob's ring and a fader's frame are lit all the time, and its
    // switches are held at zero on their own number so they still rest dark.
    for (auto const& theme : glass::BuiltInThemes())
    {
        if (theme.Name == L"Airy System")
        {
            VERIFY_IS_GREATER_THAN(theme.RestingGlowPercent, 0);
            VERIFY_ARE_EQUAL(0, theme.SwitchRestingGlowPercent);
            continue;
        }

        if (!IsTubeTheme(theme))
        {
            VERIFY_ARE_EQUAL(0, theme.RestingGlowPercent);
        }
    }
}

void ThemeTests::ATubeThemeLightsUpInAColorThatIsNeitherTheHueNorWhite()
{
    // The reason the light source had to become a color rather than stay an enum. A phosphor's
    // afterglow is a different color from its emission, and on P3 it is REDDER than the thing
    // casting it - which is the single fact people recognize as "the amber screen".
    struct Expected
    {
        wchar_t const* Name;
        glass::ThemeColor Bloom;
    };

    Expected const expected[]
    {
        { L"Cathode", { 0xCF, 0xE0, 0xF3, 255 } },
        { L"Terminal Amber", { 0xFF, 0x60, 0x10, 255 } },
        { L"Terminal Green", { 0x86, 0xF2, 0x60, 255 } },
    };

    for (auto const& entry : expected)
    {
        auto const* theme = glass::FindBuiltInTheme(entry.Name);
        VERIFY_IS_NOT_NULL(theme);

        VERIFY_IS_TRUE(glass::HasNamedBloomColor(*theme));
        VERIFY_IS_TRUE((glass::NamedBloomColor(*theme) == entry.Bloom));

        // The old enum stays where it was, so a theme file written before this existed reads
        // back identically.
        VERIFY_IS_TRUE(theme->Light == glass::LightSource::ControlHue);

        glass::Control control{};
        control.HueSlot = 0;

        auto const colors = glass::ResolveControlColors(control, *theme);

        VERIFY_ARE_EQUAL(entry.Bloom.R, colors.Bloom.R);
        VERIFY_ARE_EQUAL(entry.Bloom.G, colors.Bloom.G);
        VERIFY_ARE_EQUAL(entry.Bloom.B, colors.Bloom.B);
    }

    // Amber's halo really is redder than its own default slot, which is the whole claim.
    auto const* amber = glass::FindBuiltInTheme(L"Terminal Amber");
    auto const halo = glass::NamedBloomColor(*amber);

    VERIFY_IS_LESS_THAN(static_cast<int32_t>(halo.G), static_cast<int32_t>(amber->HueSlots[0].G));
    VERIFY_IS_LESS_THAN(static_cast<int32_t>(halo.B), static_cast<int32_t>(amber->HueSlots[0].B));

    // A theme that names nothing still lights up in the control's own hue, so nothing shipped
    // before this moved.
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_FALSE(glass::HasNamedBloomColor(*studio));
}

void ThemeTests::EveryThemeThatDerivesAColorStillResolvesToSomething()
{
    // Four properties mean "work it out" when their alpha is zero. A derived color that came
    // back transparent would be an invisible arc, a plate with no lift or a vignette that does
    // nothing, and all three are the kind of thing only one theme shows.
    for (auto const& theme : glass::BuiltInThemes())
    {
        VERIFY_IS_TRUE(glass::EffectiveArcTrackColor(theme).A != 0);
        VERIFY_IS_TRUE(glass::EffectivePlateSheenColor(theme).A != 0);
        VERIFY_IS_TRUE(glass::EffectiveVignetteColor(theme).A != 0);
        VERIFY_IS_TRUE(glass::EffectiveFaceplateColor(theme).A != 0);

        // A theme that names neither still hands back its slot track rather than nothing.
        if (theme.ArcTrackColor.A == 0)
        {
            VERIFY_IS_TRUE((glass::EffectiveArcTrackColor(theme) == theme.TrackColor));
        }
    }

    // Bone's arc track is the mirror image of a tube's. On a tube the arc sits on dark glass and
    // has to be a faint LIGHT; on Bone it sits on a near-white deck and has to be a DARK, and
    // darker than the slot track at that, or a knob shows where it is without ever showing how
    // far it can go. Measured as it lands rather than as it is written: both are alpha over the
    // deck, so comparing the two written colors compares nothing.
    auto const* bone = glass::FindBuiltInTheme(L"Bone");
    VERIFY_IS_NOT_NULL(bone);

    VERIFY_IS_TRUE(bone->ArcTrackColor.A != 0);

    auto const boneArc = glass::BlendOver(
        bone->Deck.GradientEndColor, glass::EffectiveArcTrackColor(*bone), 1.0);

    auto const boneSlot = glass::BlendOver(bone->Deck.GradientEndColor, bone->TrackColor, 1.0);

    Log::Comment(String().Format(L"Bone arc track %.3f, slot track %.3f",
        glass::RelativeLuminance(boneArc), glass::RelativeLuminance(boneSlot)));

    VERIFY_IS_LESS_THAN(glass::RelativeLuminance(boneArc), glass::RelativeLuminance(boneSlot));

    // And the three tubes go the other way: the arc is brighter than the glass behind it.
    for (auto const* name : TubeThemeNames)
    {
        auto const* theme = glass::FindBuiltInTheme(name);

        auto const arc = glass::BlendOver(
            theme->Deck.GradientEndColor, glass::EffectiveArcTrackColor(*theme), 1.0);

        VERIFY_IS_GREATER_THAN(
            glass::RelativeLuminance(arc),
            glass::RelativeLuminance(theme->Deck.GradientEndColor));
    }
}

void ThemeTests::EveryMeterZoneNamesASlotThatExists()
{
    for (auto const& theme : glass::BuiltInThemes())
    {
        for (int32_t zone = 0; zone < glass::MeterZoneCount; ++zone)
        {
            auto const slot = theme.MeterSlots[static_cast<size_t>(zone)];

            VERIFY_IS_GREATER_THAN_OR_EQUAL(slot, 0);
            VERIFY_IS_LESS_THAN(slot, glass::ThemeHueSlotCount);
        }

        glass::Control control{};
        control.HueSlot = 0;

        auto const colors = glass::ResolveControlColors(control, theme);

        // Each zone is the slot the theme named, not the control's own hue. A meter drawn in
        // one color is the one control where the theme's choice is doing real work.
        VERIFY_IS_TRUE((colors.MeterLit == theme.HueSlots[static_cast<size_t>(theme.MeterSlots[0])]));
        VERIFY_IS_TRUE((colors.MeterWarn == theme.HueSlots[static_cast<size_t>(theme.MeterSlots[1])]));
        VERIFY_IS_TRUE((colors.MeterHot == theme.HueSlots[static_cast<size_t>(theme.MeterSlots[2])]));

        // On a theme with color to spend, green, amber and red is the convention and red is
        // legitimately darker than green. On a tube there is no color to spend, so the three
        // zones have to be three brightnesses or the meter says nothing at all.
        if (IsTubeTheme(theme))
        {
            Log::Comment(String().Format(L"%s meter zones: %.3f -> %.3f -> %.3f",
                theme.Name.c_str(),
                glass::RelativeLuminance(colors.MeterLit),
                glass::RelativeLuminance(colors.MeterWarn),
                glass::RelativeLuminance(colors.MeterHot)));

            VERIFY_IS_GREATER_THAN(
                glass::RelativeLuminance(colors.MeterWarn),
                glass::RelativeLuminance(colors.MeterLit));

            VERIFY_IS_GREATER_THAN(
                glass::RelativeLuminance(colors.MeterHot),
                glass::RelativeLuminance(colors.MeterWarn));
        }
    }
}

void ThemeTests::CathodeCannotColorCodeAndSaysSo()
{
    auto const* cathode = glass::FindBuiltInTheme(L"Cathode");
    VERIFY_IS_NOT_NULL(cathode);

    // Six brightnesses of one phosphor. The widest pair in this palette does not reach the 3 : 1
    // somebody needs to tell two controls apart, so on this theme a control is known by where it
    // sits and what it is called. That is a property of a tube, not a defect to fix.
    auto widest = 1.0;

    for (int32_t first = 0; first < glass::ThemeHueSlotCount; ++first)
    {
        for (int32_t second = first + 1; second < glass::ThemeHueSlotCount; ++second)
        {
            widest = (std::max)(widest, glass::ContrastRatio(
                cathode->HueSlots[static_cast<size_t>(first)],
                cathode->HueSlots[static_cast<size_t>(second)]));
        }
    }

    Log::Comment(String().Format(L"Cathode's widest pair of slots measures %.2f : 1", widest));
    VERIFY_IS_LESS_THAN(widest, 3.0);

    // So the picker has to say so. A theme that cannot carry grouping by color and does not
    // admit it is worse than one that never tried.
    VERIFY_IS_FALSE(cathode->CautionResourceKey.empty());

    // The glass is olive - red and blue equal - which is what separates it from the green
    // terminal. Built as green on dark green the two stop looking like different ideas.
    VERIFY_ARE_EQUAL(cathode->Deck.Color.R, cathode->Deck.Color.B);
    VERIFY_IS_GREATER_THAN(
        static_cast<int32_t>(cathode->Deck.Color.G), static_cast<int32_t>(cathode->Deck.Color.R));
}

void ThemeTests::TheAmberRampIsARisingBrightnessAsWellAsARisingHue()
{
    // What keeps the theme honest with no color vision at all. The ramp runs ember, orange,
    // amber, standard, yellow, white hot, and every step up that ladder is also brighter, so
    // stripping the color out leaves a wider ladder than Cathode's rather than six of the same.
    auto const* amber = glass::FindBuiltInTheme(L"Terminal Amber");
    VERIFY_IS_NOT_NULL(amber);

    // Ramp order, which is not slot order: h1 stays the default slot.
    constexpr size_t rampOrder[]{ 3, 1, 4, 0, 2, 5 };

    auto previous = 0.0;

    for (auto const slot : rampOrder)
    {
        auto const luminance = glass::RelativeLuminance(amber->HueSlots[slot]);

        Log::Comment(String().Format(
            L"Amber slot %d measures %.3f relative luminance", static_cast<int32_t>(slot) + 1, luminance));

        VERIFY_IS_GREATER_THAN(luminance, previous);
        previous = luminance;
    }

    // Long persistence is a property of the tube rather than of one binding.
    VERIFY_IS_GREATER_THAN(amber->PersistenceMilliseconds, 220);

    // The fill runs into the halo, which is what makes an amber fader look like fire.
    VERIFY_IS_TRUE(amber->ValueFadesToLight);
}

void ThemeTests::TerminalGreenGlassIsBlueSlateRatherThanGreen()
{
    // The one that would have been got wrong from memory. A terminal has a tinted anti-glare
    // faceplate, so the unlit screen is a cool blue-slate and the green sits a long way from it
    // in hue. Built as green on dark green it comes out as Cathode with the colors swapped.
    auto const* green = glass::FindBuiltInTheme(L"Terminal Green");
    VERIFY_IS_NOT_NULL(green);

    VERIFY_IS_GREATER_THAN(
        static_cast<int32_t>(green->Deck.Color.B), static_cast<int32_t>(green->Deck.Color.R));

    // The afterglow is yellower than the emission, the same way Amber's is redder.
    auto const halo = glass::NamedBloomColor(*green);

    VERIFY_IS_GREATER_THAN(
        static_cast<int32_t>(halo.R), static_cast<int32_t>(green->HueSlots[0].R));
}

void ThemeTests::NothingShippedReachesAPureBlackOrAPureWhite()
{
    // A true black punches a hole in a piece of glass and a true white looks like paper. The
    // three tube themes are built so that neither ever happens - the palette floors and ceilings
    // were measured against a rendered deck, and this is the guard that keeps them there.
    for (auto const* name : TubeThemeNames)
    {
        auto const* theme = glass::FindBuiltInTheme(name);
        VERIFY_IS_NOT_NULL(theme);

        glass::ThemeColor const checked[]
        {
            theme->Deck.Color,
            theme->Deck.GradientEndColor,
            theme->PlateColor,
            theme->PlateEndColor,
            theme->Overlay.VignetteColor,
            theme->Overlay.ScanLineColor,
        };

        for (auto const& color : checked)
        {
            VERIFY_IS_TRUE(color.R != 0 || color.G != 0 || color.B != 0);
            VERIFY_IS_TRUE(color.R != 255 || color.G != 255 || color.B != 255);
        }

        for (auto const& slot : theme->HueSlots)
        {
            VERIFY_IS_TRUE(slot.R != 255 || slot.G != 255 || slot.B != 255);
        }
    }
}

// ============================================================================
// The two hardware panels, and the engine work they earned.
//
// These two break the assumption every theme before them shares: that a control is a plate with
// a hairline of its own color on it. Jove has no plate on a knob at all and its tabs ARE their
// color at rest; Supersaw says "on" with three lit pixels and leaves the plate black.
// ============================================================================

namespace
{
    wchar_t const* const PanelThemeNames[]{ L"Jove", L"Supersaw" };

    // The second pair of instrument panels. They carry a grain and a neutral as well, but they
    // are lit from above and one of them has a rim, so they are not held to the first pair's
    // rules about labels, rims and flat decks.
    wchar_t const* const PrintedPanelThemeNames[]{ L"Five-iSH", L"Airy System" };

    bool IsPanelTheme(_In_ glass::Theme const& theme) noexcept
    {
        for (auto const* name : PanelThemeNames)
        {
            if (theme.Name == name)
            {
                return true;
            }
        }

        return false;
    }

    bool IsPrintedPanelTheme(_In_ glass::Theme const& theme) noexcept
    {
        for (auto const* name : PrintedPanelThemeNames)
        {
            if (theme.Name == name)
            {
                return true;
            }
        }

        return false;
    }
}

void ThemeTests::OnlyAPanelThemeCarriesAGrainOrANeutral()
{
    // Both are additive and both default to off, so every theme shipped before these two has to
    // come out untouched. A grain on Studio Dark would be a defect nobody would look for.
    for (auto const& theme : glass::BuiltInThemes())
    {
        if (IsPanelTheme(theme) || IsPrintedPanelTheme(theme))
        {
            continue;
        }

        VERIFY_ARE_EQUAL(0, theme.Overlay.GrainPercent);
        VERIFY_IS_FALSE(glass::HasNeutralColor(theme));
    }

    // Everything about how a switch, a knob, a fader and a section are drawn moved off the
    // original look only on the themes drawn from a comp that asked for it. The rest have
    // to come out exactly as they were, because every property here is additive: a theme that
    // picked one of these up by accident is a theme that changed with nobody deciding it should.
    wchar_t const* const drawnFromAComp[]
    {
        L"Jove", L"Supersaw", L"Bigwig", L"Bone", L"Five-iSH", L"Airy System",
    };

    auto const fromAComp = [&drawnFromAComp](std::wstring const& name)
        {
            return std::find_if(std::begin(drawnFromAComp), std::end(drawnFromAComp),
                [&name](wchar_t const* one) { return name == one; }) != std::end(drawnFromAComp);
        };

    for (auto const& theme : glass::BuiltInThemes())
    {
        if (fromAComp(theme.Name))
        {
            continue;
        }

        Log::Comment(String().Format(L"%s keeps the original look", theme.Name.c_str()));

        VERIFY_IS_TRUE(theme.SectionHeader == glass::SectionHeaderStyle::Caption);
        VERIFY_ARE_EQUAL(34, theme.FillWhenOnPercent);
        VERIFY_IS_LESS_THAN(theme.SwitchFillAtRest, 0.0);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.PointerColor.A);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.CapLineColor.A);

        VERIFY_IS_FALSE(theme.SectionNameInHue);
        VERIFY_IS_TRUE(theme.PanelFill == glass::PanelFillStyle::Plate);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.PanelOutlineColor.A);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.KnobFaceColor.A);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.KnobCapColor.A);
        VERIFY_ARE_EQUAL(0, theme.KnobTickCount);
        VERIFY_IS_FALSE(theme.NamesInsideSwitches);
        VERIFY_ARE_EQUAL(0, theme.OnLiftPercent);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.LampColor.A);
        VERIFY_ARE_EQUAL(0, theme.PlateShadePercent);
        VERIFY_ARE_EQUAL(0, theme.PlateHighlightPercent);
        VERIFY_IS_TRUE(theme.FaderPlate == glass::FaderPlateStyle::Full);
        VERIFY_ARE_EQUAL(100, theme.FaderFillPercent);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.ValueColor.A);
        VERIFY_ARE_EQUAL(0, theme.RecessShadePercent);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.WellColor.A);
        VERIFY_ARE_EQUAL(0, theme.ThumbShadowPercent);
        VERIFY_IS_FALSE(theme.CapLineWide);

        // and none of what Five-iSH and Airy System added
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.InsetPanelColor.A);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.SectionInkColor.A);
        VERIFY_ARE_EQUAL(-1, theme.PanelElevation);
        VERIFY_IS_FALSE(theme.PointerOnCap);
        VERIFY_IS_FALSE(theme.ArcGlow);
        VERIFY_ARE_EQUAL(0, theme.ArcTrackHuePercent);
        VERIFY_IS_TRUE(theme.LampShape == glass::LampStyle::Bar);
        VERIFY_ARE_EQUAL(-1, theme.SwitchRimStrengthPercent);
        VERIFY_ARE_EQUAL(-1, theme.SwitchRestingGlowPercent);
        VERIFY_IS_LESS_THAN(theme.PadFillAtRest, 0.0);
        VERIFY_ARE_EQUAL(-1, theme.PadFillWhenOnPercent);
        VERIFY_IS_FALSE(theme.NeutralCaps);
        VERIFY_ARE_EQUAL(0, theme.FaderScalePercent);
        VERIFY_ARE_EQUAL(1, theme.Overlay.GrainStreak);
    }

    // The grain is the panel's texture rather than a second color over it, so it belongs to
    // exactly one of the two.
    auto const* saw = glass::FindBuiltInTheme(L"Supersaw");
    VERIFY_IS_NOT_NULL(saw);
    VERIFY_IS_GREATER_THAN(saw->Overlay.GrainPercent, 0);
    VERIFY_IS_FALSE(saw->Overlay.IsEmpty());
}

void ThemeTests::JoveFillsItsSwitchesWithoutFillingItsKnobs()
{
    auto const* jove = glass::FindBuiltInTheme(L"Jove");
    VERIFY_IS_NOT_NULL(jove);

    // One number for the whole theme could not say this: turning the tabs on filled the knobs
    // too. That is the entire reason the property exists.
    VERIFY_ARE_EQUAL(0.0, jove->FillAtRest);
    VERIFY_IS_GREATER_THAN(jove->SwitchFillAtRest, 0.5);

    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;
    knob.HueSlot = 0;

    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;
    tab.HueSlot = 0;

    auto const knobColors = glass::ResolveControlColors(knob, *jove);
    auto const tabColors = glass::ResolveControlColors(tab, *jove);

    auto const hue = jove->HueSlots[0];

    // The knob is the black cap the theme named. The tab IS its own color at rest, and lit it is
    // the same plastic with the lamp behind it on: paler, never darker.
    VERIFY_IS_TRUE((knobColors.Plate == jove->PlateColor));
    VERIFY_IS_TRUE((tabColors.Plate == hue));
    VERIFY_IS_GREATER_THAN(glass::RelativeLuminance(tabColors.OnPlate), glass::RelativeLuminance(hue));
    VERIFY_IS_GREATER_THAN(jove->OnLiftPercent, 0);

    // and the two really are different, which is what a single fill number could not produce
    VERIFY_IS_FALSE((knobColors.Plate == tabColors.Plate));

    // On this theme a tab's label sits ON its color rather than beside it, so every slot has to
    // carry type at 4.5 as well as clear the panel at 3.0. That is a harder bar than any other
    // shipped theme has to meet, and it is what a solid-color switch costs.
    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        auto const& color = jove->HueSlots[static_cast<size_t>(slot)];
        auto const ratio = glass::ContrastRatio(glass::ReadableInk(color), color);

        if (ratio < 4.5)
        {
            Log::Error(String().Format(
                L"Jove slot %d carries its own label at only %.2f : 1.", slot + 1, ratio));
        }

        VERIFY_IS_GREATER_THAN(ratio, 4.5);
    }

    // The cream tabs are the seventh color and they carry type too.
    VERIFY_IS_GREATER_THAN(
        glass::ContrastRatio(glass::ReadableInk(jove->NeutralColor), jove->NeutralColor), 4.5);
}

void ThemeTests::SupersawSaysOnWithItsLampRatherThanItsPlate()
{
    auto const* saw = glass::FindBuiltInTheme(L"Supersaw");
    VERIFY_IS_NOT_NULL(saw);

    VERIFY_ARE_EQUAL(0, saw->FillWhenOnPercent);

    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;
    tab.HueSlot = 1;

    auto const colors = glass::ResolveControlColors(tab, *saw);

    // On is not a plate state here. A switch that is on looks exactly like one that is off
    // apart from its lamp, which is what makes a rack of forty of them readable.
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.OnPlate.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.OnPlateEnd.A);

    // Every theme that fills is untouched by the same arithmetic.
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);

    auto const filled = glass::ResolveControlColors(tab, *studio);
    VERIFY_IS_GREATER_THAN(static_cast<int32_t>(filled.OnPlate.A), 0);
}

void ThemeTests::TheNeutralSlotIsTheAbsenceOfAColorRatherThanASeventhHue()
{
    auto const* jove = glass::FindBuiltInTheme(L"Jove");
    VERIFY_IS_NOT_NULL(jove);

    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;
    tab.HueSlot = glass::NeutralSlot;

    VERIFY_IS_TRUE(glass::IsSlotInRange(tab.HueSlot));
    VERIFY_IS_TRUE((glass::ResolveHue(tab, *jove) == jove->NeutralColor));

    // It is not one of the six. A customer counting seven button colors on a photograph has six
    // hues and one absence, and putting the absence in a slot is what survives a theme swap.
    for (auto const& slot : jove->HueSlots)
    {
        VERIFY_IS_FALSE((slot == jove->NeutralColor));
    }

    // A theme with no neutral falls the control back to a color rather than to nothing, because
    // an invisible control is worse than a wrongly colored one.
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);
    VERIFY_IS_FALSE(glass::HasNeutralColor(*studio));
    VERIFY_IS_TRUE((glass::ResolveHue(tab, *studio) == studio->HueSlots[0]));
}

void ThemeTests::APointerAndACapLineCanStopFollowingTheHue()
{
    // The mirror of the white-and-black rule: anything worked out from the CONTROL'S HUE
    // eventually meets a theme where it is not the hue. Both of these were derived until a panel
    // arrived whose every knob points in one color and whose every cap line is white.
    auto const* jove = glass::FindBuiltInTheme(L"Jove");
    VERIFY_IS_NOT_NULL(jove);

    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;

    glass::Control fader{};
    fader.Kind = glass::ControlKind::Fader;

    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        knob.HueSlot = slot;
        fader.HueSlot = slot;

        VERIFY_IS_TRUE((glass::ResolveControlColors(knob, *jove).Pointer == jove->PointerColor));
        VERIFY_IS_TRUE((glass::ResolveControlColors(fader, *jove).ThumbLine == jove->CapLineColor));
    }

    // and a theme that names neither still follows the hue, which is what every shipped theme
    // did before this existed
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);

    knob.HueSlot = 3;
    VERIFY_IS_TRUE((glass::ResolveControlColors(knob, *studio).Pointer == studio->HueSlots[3]));
}

void ThemeTests::EveryPanelThemePutsItsLabelsAboveItsControls()
{
    // Silkscreen sits above what it names. Both comps asked for it, which is what turned it from
    // one theme's preference into a fourth value on a property that already existed.
    for (auto const* name : PanelThemeNames)
    {
        auto const* theme = glass::FindBuiltInTheme(name);
        VERIFY_IS_NOT_NULL(theme);

        VERIFY_IS_TRUE(theme->Labels == glass::LabelPlacement::Above);

        // Neither of them has a rim. The control IS the part you touch, so there is nothing for
        // a hairline to sit on.
        VERIFY_IS_TRUE(theme->Rim == glass::RimSource::None);

        // Both name their plate outright rather than deriving it from a glass tint.
        VERIFY_IS_GREATER_THAN(static_cast<int32_t>(theme->PlateColor.A), 0);
        VERIFY_ARE_EQUAL(0, theme->GlassTintPercent);

        // A panel is painted, not lit from above like a sheet of glass.
        VERIFY_IS_TRUE(theme->Deck.Kind == glass::DeckKind::SolidColor);
    }
}

void ThemeTests::NoShippedThemeGoesDarkerWhenItIsLit()
{
    // A switch turning on has to say something, whichever direction it says it in. The wash used
    // to be laid over NOTHING rather than over the resting plate, which is right only while that
    // plate is dark glass, and it failed at both ends: on Bone a lit plate went 247,243,232 ->
    // 193,183,166, and on a theme whose tabs are already their own color at full strength the
    // lit state came out identical to the resting one.
    //
    // So the bar is "did it move", not "did it get brighter". A light theme legitimately goes
    // darker - a colored button on paper - and that reads correctly.
    for (auto const& theme : glass::BuiltInThemes())
    {
        for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
        {
            glass::Control tab{};
            tab.Kind = glass::ControlKind::Toggle;
            tab.HueSlot = slot;

            auto const colors = glass::ResolveControlColors(tab, theme);

            // Alpha 0 means the theme says "on" with its lamp and never touches the plate.
            if (colors.OnPlate.A == 0)
            {
                VERIFY_ARE_EQUAL(0, theme.FillWhenOnPercent);
                continue;
            }

            auto const resting = colors.Plate.A == 0
                ? theme.Deck.Color
                : glass::BlendOver(theme.Deck.Color, colors.Plate, 1.0);

            auto const moved = glass::ContrastRatio(colors.OnPlate, resting);

            if (moved < 1.15)
            {
                Log::Error(String().Format(
                    L"%s slot %d looks the same lit as it does at rest (%.3f : 1).",
                    theme.Name.c_str(), slot + 1, moved));
            }

            VERIFY_IS_GREATER_THAN_OR_EQUAL(moved, 1.15);
        }
    }
}

void ThemeTests::APointerIsVisibleOnEveryThemesOwnPlate()
{
    // A knob's pointer is a two pixel line, so it needs the graphics bar of 3.0 against whatever
    // it is drawn on. It used to be the control's hue, which could never fail this because a hue
    // has to clear the deck anyway. Once a theme could NAME it, it could name one that vanishes -
    // and measuring at 5x magnification is exactly where an eye says "that looks light" about a
    // line that is actually 1.4 : 1.
    for (auto const& theme : glass::BuiltInThemes())
    {
        glass::Control knob{};
        knob.Kind = glass::ControlKind::Knob;
        knob.HueSlot = 0;

        auto const colors = glass::ResolveControlColors(knob, theme);

        // A knob with a turned face has its pointer drawn on the face, not on the plate, and a
        // pointer printed on the cap is drawn on the cap.
        auto const behind = theme.PointerOnCap && colors.KnobCap.A != 0
            ? glass::BlendOver(colors.KnobCap, colors.KnobCapEnd, 0.5)
            : (theme.KnobFaceColor.A != 0
                ? glass::BlendOver(colors.KnobFace, colors.KnobFaceEnd, 0.5)
                : (colors.Plate.A == 0
                    ? theme.Deck.Color
                    : glass::BlendOver(theme.Deck.Color, colors.Plate, 1.0)));

        auto const ratio = glass::ContrastRatio(colors.Pointer, behind);

        if (ratio < glass::MinimumSlotContrast)
        {
            Log::Error(String().Format(
                L"%s draws a knob pointer at only %.2f : 1 against its own plate.",
                theme.Name.c_str(), ratio));
        }

        VERIFY_IS_GREATER_THAN_OR_EQUAL(ratio, glass::MinimumSlotContrast);
    }
}

// ============================================================================
// The engine round that brought Bone, Bigwig, Jove and Supersaw up to their comps.
// ============================================================================

void ThemeTests::TheTouchRimIsTheRimComingUpNotBlack()
{
    // The touch rim used to be worked out before the rim itself, while the rim was still a
    // default ThemeColor - opaque black - so a finger on any control turned its rim black.
    for (auto const& theme : glass::BuiltInThemes())
    {
        glass::Control fader{};
        fader.Kind = glass::ControlKind::Fader;
        fader.HueSlot = 0;

        auto const colors = glass::ResolveControlColors(fader, theme);

        if (colors.Rim.A == 0)
        {
            VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.TouchRim.A);
            continue;
        }

        VERIFY_ARE_EQUAL(colors.Rim.R, colors.TouchRim.R);
        VERIFY_ARE_EQUAL(colors.Rim.G, colors.TouchRim.G);
        VERIFY_ARE_EQUAL(colors.Rim.B, colors.TouchRim.B);

        // Up, never down: a finger makes the rim stronger than it is at rest.
        VERIFY_IS_GREATER_THAN_OR_EQUAL(colors.TouchRim.A, colors.Rim.A);
    }
}

void ThemeTests::ASwitchNameReadsOnItsPlateLitOrNot()
{
    // A name printed on a switch sits on its plate, and on a theme whose lit switch is its color
    // outright that plate changes completely. So the name is measured against both, and a
    // theme's own ink is only used where it actually reads. Checked on every theme that prints
    // names on its switches.
    for (auto const& theme : glass::BuiltInThemes())
    {
        if (!theme.NamesInsideSwitches && theme.Labels != glass::LabelPlacement::Inside)
        {
            continue;
        }

        for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
        {
            glass::Control tab{};
            tab.Kind = glass::ControlKind::Toggle;
            tab.HueSlot = slot;

            auto const colors = glass::ResolveControlColors(tab, theme);

            auto const restTop = colors.Plate.A == 0
                ? theme.Deck.Color
                : glass::BlendOver(theme.Deck.Color, colors.Plate, 1.0);

            auto const restBottom = colors.PlateEnd.A == 0
                ? theme.Deck.GradientEndColor
                : glass::BlendOver(theme.Deck.GradientEndColor, colors.PlateEnd, 1.0);

            auto const resting = glass::BlendOver(restTop, restBottom, 0.5);
            auto const atRest = glass::ContrastRatio(colors.SwitchInk, resting);

            if (atRest < 4.5)
            {
                Log::Error(String().Format(L"%s slot %d names a switch at %.2f : 1 at rest.",
                    theme.Name.c_str(), slot + 1, atRest));
            }

            VERIFY_IS_GREATER_THAN_OR_EQUAL(atRest, 4.5);

            if (colors.OnPlate.A == 0)
            {
                // A lamp theme leaves the plate alone, so the name does not change either.
                VERIFY_IS_TRUE((colors.SwitchInkOn == colors.SwitchInk));
                continue;
            }

            auto const lit = glass::ContrastRatio(
                colors.SwitchInkOn, glass::BlendOver(colors.OnPlate, colors.OnPlateEnd, 0.5));

            if (lit < 4.5)
            {
                Log::Error(String().Format(L"%s slot %d names a lit switch at %.2f : 1.",
                    theme.Name.c_str(), slot + 1, lit));
            }

            VERIFY_IS_GREATER_THAN_OR_EQUAL(lit, 4.5);
        }
    }
}

void ThemeTests::AFullFillIsTheHueTopToBottom()
{
    // At a third of a fill the bottom of a lit plate carries half as much hue, which is the lit
    // top edge every theme was drawn with. At a full fill it catches up: a tab that IS its color
    // when it is on is that color from top to bottom rather than fading out.
    auto theme = *glass::FindBuiltInTheme(L"Studio Dark");

    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;
    tab.HueSlot = 0;

    theme.FillWhenOnPercent = 100;

    auto const full = glass::ResolveControlColors(tab, theme);

    VERIFY_IS_TRUE((full.OnPlate == theme.HueSlots[0]));
    VERIFY_IS_TRUE((full.OnPlateEnd == theme.HueSlots[0]));

    // and the original third is exactly what it always was
    theme.FillWhenOnPercent = 34;

    auto const third = glass::ResolveControlColors(tab, theme);

    auto const restingBottom = glass::BlendOver(theme.Deck.GradientEndColor, third.PlateEnd, 1.0);
    auto expected = glass::BlendOver(restingBottom, theme.HueSlots[0], 0.17);
    expected.A = 255;

    VERIFY_IS_TRUE((third.OnPlateEnd == expected));

    // Lifting goes toward white and deepening toward black, and both leave the resting plate
    // alone.
    theme.FillWhenOnPercent = 100;
    theme.OnLiftPercent = 30;

    auto const lifted = glass::ResolveControlColors(tab, theme);
    VERIFY_IS_GREATER_THAN(
        glass::RelativeLuminance(lifted.OnPlate), glass::RelativeLuminance(theme.HueSlots[0]));

    theme.OnLiftPercent = -30;

    auto const deepened = glass::ResolveControlColors(tab, theme);
    VERIFY_IS_LESS_THAN(
        glass::RelativeLuminance(deepened.OnPlate), glass::RelativeLuminance(theme.HueSlots[0]));

    VERIFY_IS_TRUE((lifted.Plate == deepened.Plate));
}

void ThemeTests::AnLfoIsNeverFilledLikeASwitch()
{
    // Its plate is where the wave is drawn. On a theme whose tabs are solid color at rest, an LFO
    // filled like a tab put a red wave on a red plate, and a wave the same color as the plate
    // under it is not there at all.
    VERIFY_IS_TRUE(glass::IsSwitchControl(glass::ControlKind::Lfo));
    VERIFY_IS_FALSE(glass::FillsLikeASwitch(glass::ControlKind::Lfo));
    VERIFY_IS_TRUE(glass::FillsLikeASwitch(glass::ControlKind::Toggle));

    auto const* jove = glass::FindBuiltInTheme(L"Jove");
    VERIFY_IS_NOT_NULL(jove);

    glass::Control lfo{};
    lfo.Kind = glass::ControlKind::Lfo;
    lfo.HueSlot = 0;

    auto const colors = glass::ResolveControlColors(lfo, *jove);

    VERIFY_IS_TRUE((colors.Plate == jove->PlateColor));

    // and the wave on it reads
    auto const behind = jove->WellColor.A != 0 ? jove->WellColor : jove->PlateColor;
    VERIFY_IS_GREATER_THAN_OR_EQUAL(glass::ContrastRatio(colors.Pipe, behind), glass::MinimumSlotContrast);
}

void ThemeTests::AFaderFillFollowsTheThemesStrength()
{
    glass::Control fader{};
    fader.Kind = glass::ControlKind::Fader;
    fader.HueSlot = 0;

    // Every theme that fills a fader outright still does.
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    auto const full = glass::ResolveControlColors(fader, *studio);

    VERIFY_IS_TRUE((full.Fill == full.Pipe));
    VERIFY_IS_TRUE((full.FillEnd == full.PipeEnd));

    // A panel whose cap position is the whole value lights the slot below the cap only faintly.
    auto const* jove = glass::FindBuiltInTheme(L"Jove");
    auto const faint = glass::ResolveControlColors(fader, *jove);

    VERIFY_IS_LESS_THAN(jove->FaderFillPercent, 100);
    VERIFY_ARE_EQUAL(
        static_cast<int32_t>(std::lround(faint.Pipe.A * jove->FaderFillPercent / 100.0)),
        static_cast<int32_t>(faint.Fill.A));

    // The color does not move, only its strength.
    VERIFY_ARE_EQUAL(faint.Pipe.R, faint.Fill.R);
    VERIFY_ARE_EQUAL(faint.Pipe.G, faint.Fill.G);
    VERIFY_ARE_EQUAL(faint.Pipe.B, faint.Fill.B);
}

void ThemeTests::TheValueColorDrawsEveryValueInOneColor()
{
    auto const* jove = glass::FindBuiltInTheme(L"Jove");
    VERIFY_IS_NOT_NULL(jove);
    VERIFY_IS_TRUE(jove->ValueColor.A != 0);

    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        glass::Control knob{};
        knob.Kind = glass::ControlKind::Knob;
        knob.HueSlot = slot;

        glass::Control tab{};
        tab.Kind = glass::ControlKind::Toggle;
        tab.HueSlot = slot;

        // Every value is the section orange, whatever the control's own color...
        VERIFY_IS_TRUE((glass::ResolveControlColors(knob, *jove).Pipe == jove->ValueColor));

        // ...and a tab is still its own color, because a tab is not a value.
        VERIFY_IS_TRUE((glass::ResolveControlColors(tab, *jove).Plate == jove->HueSlots[static_cast<size_t>(slot)]));
    }

    // A theme that names none still draws each value in its control's own hue.
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");

    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;
    knob.HueSlot = 3;

    VERIFY_IS_TRUE((glass::ResolveControlColors(knob, *studio).Pipe == studio->HueSlots[3]));
}

void ThemeTests::EveryKeyboardHasALightAndADarkKey()
{
    // The naturals are the theme's light and the sharps its dark, so the two have to be told
    // apart at a glance on every theme, including the ones with no white or black at all.
    for (auto const& theme : glass::BuiltInThemes())
    {
        auto const white = glass::EffectiveKeyWhiteColor(theme);
        auto const black = glass::EffectiveKeyBlackColor(theme);

        auto const ratio = glass::ContrastRatio(white, black);

        Log::Comment(String().Format(L"%s keys measure %.2f : 1", theme.Name.c_str(), ratio));

        VERIFY_IS_GREATER_THAN(glass::RelativeLuminance(white), glass::RelativeLuminance(black));
        VERIFY_IS_GREATER_THAN_OR_EQUAL(ratio, glass::MinimumSlotContrast);
    }

    // The tubes have no white and no black, so theirs are the phosphor and the glass.
    for (auto const* name : TubeThemeNames)
    {
        auto const* theme = glass::FindBuiltInTheme(name);

        VERIFY_IS_TRUE(theme->KeyWhiteColor.A != 0);
        VERIFY_IS_TRUE(theme->KeyBlackColor.A != 0);
    }

    // A theme that names neither still draws the plain keyboard it always did.
    glass::Theme plain{};

    VERIFY_IS_TRUE((glass::EffectiveKeyWhiteColor(plain) == glass::ThemeColor{ 232, 234, 238, 255 }));
    VERIFY_IS_TRUE((glass::EffectiveKeyBlackColor(plain) == glass::ThemeColor{ 22, 25, 31, 255 }));
}

void ThemeTests::ALampThemeLightsItsOwnLampColor()
{
    auto const* saw = glass::FindBuiltInTheme(L"Supersaw");
    VERIFY_IS_NOT_NULL(saw);

    // A lamp is the one light on the switch, and it is the same red whatever slot the switch
    // is on. Lit, a faint line of that red goes round the switch, and the glow is red too.
    VERIFY_IS_TRUE(saw->LampColor.A != 0);

    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        glass::Control tab{};
        tab.Kind = glass::ControlKind::Toggle;
        tab.HueSlot = slot;

        auto const colors = glass::ResolveControlColors(tab, *saw);

        VERIFY_IS_TRUE((colors.Lamp == saw->LampColor));
        VERIFY_IS_GREATER_THAN(static_cast<int32_t>(colors.LampRim.A), 0);
        VERIFY_IS_LESS_THAN(static_cast<int32_t>(colors.LampRim.A), 255);
        VERIFY_ARE_EQUAL(saw->LampColor.R, colors.Bloom.R);
    }

    // and a strip as well as a lamp is two lights saying one thing
    VERIFY_IS_TRUE(saw->ValueStrip == glass::ValueStripPlacement::None);

    // A theme that fills has no lamp at all.
    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;

    VERIFY_ARE_EQUAL(uint8_t{ 0 }, glass::ResolveControlColors(tab, *glass::FindBuiltInTheme(L"Studio Dark")).Lamp.A);
}

void ThemeTests::ADefaultThemeAsksForNoneOfTheNewLooks()
{
    // Every property this round added is additive: a theme file written before it existed has to
    // draw exactly what it always did. These are the defaults that promise says.
    glass::Theme theme{};

    VERIFY_IS_TRUE(theme.PanelFill == glass::PanelFillStyle::Plate);
    VERIFY_IS_TRUE(theme.FaderPlate == glass::FaderPlateStyle::Full);
    VERIFY_IS_FALSE(theme.SectionNameInHue);
    VERIFY_IS_FALSE(theme.NamesInsideSwitches);
    VERIFY_IS_FALSE(theme.CapLineWide);
    VERIFY_ARE_EQUAL(100, theme.FaderFillPercent);
    VERIFY_ARE_EQUAL(0, theme.OnLiftPercent);
    VERIFY_ARE_EQUAL(0, theme.PlateShadePercent);
    VERIFY_ARE_EQUAL(0, theme.PlateHighlightPercent);
    VERIFY_ARE_EQUAL(0, theme.RecessShadePercent);
    VERIFY_ARE_EQUAL(0, theme.ThumbShadowPercent);
    VERIFY_ARE_EQUAL(0, theme.KnobTickCount);

    for (auto const& color : { theme.PanelColor, theme.PanelEndColor, theme.PanelOutlineColor,
        theme.KnobFaceColor, theme.KnobFaceEndColor, theme.KnobCapColor, theme.KnobCapEndColor,
        theme.LampColor, theme.ValueColor, theme.WellColor, theme.KeyWhiteColor, theme.KeyBlackColor })
    {
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, color.A);
    }

    // So a control on it resolves with no well, no recess, no shade and no highlight, and its
    // knob face is its plate.
    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;

    auto const colors = glass::ResolveControlColors(knob, theme);

    VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.Well.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.Recess.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.PlateShade.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.PlateHighlight.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.KnobCap.A);
    VERIFY_IS_TRUE((colors.KnobFace == colors.Plate));
    VERIFY_IS_TRUE((colors.PanelOutline == colors.Rim));

    // Nor any of what Five-iSH and Airy System added: one surface, one ink, switches that rest
    // like knobs, pads that fill like switches, and scales and rules drawn the way they were.
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.InsetPanelColor.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.InsetPanelEndColor.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.SectionInkColor.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, theme.RuleColor.A);
    VERIFY_ARE_EQUAL(-1, theme.PanelElevation);
    VERIFY_ARE_EQUAL(-1, theme.SwitchRimStrengthPercent);
    VERIFY_ARE_EQUAL(-1, theme.SwitchRestingGlowPercent);
    VERIFY_ARE_EQUAL(-1, theme.PadFillWhenOnPercent);
    VERIFY_IS_LESS_THAN(theme.PadFillAtRest, 0.0);
    VERIFY_ARE_EQUAL(0, theme.ArcTrackHuePercent);
    VERIFY_ARE_EQUAL(0, theme.FaderScalePercent);
    VERIFY_ARE_EQUAL(1, theme.Overlay.GrainStreak);
    VERIFY_IS_FALSE(theme.PointerOnCap);
    VERIFY_IS_FALSE(theme.ArcGlow);
    VERIFY_IS_FALSE(theme.NeutralCaps);
    VERIFY_IS_TRUE(theme.RuleFades);
    VERIFY_IS_TRUE(theme.LampShape == glass::LampStyle::Bar);
    VERIFY_IS_TRUE(theme.SectionHeader == glass::SectionHeaderStyle::Caption);

    VERIFY_IS_TRUE((colors.SectionLabel == colors.Label));
    VERIFY_IS_TRUE((colors.FaderTick == colors.Marks));
    VERIFY_IS_TRUE((colors.SectionRule == colors.Rule));
    VERIFY_IS_TRUE((colors.ArcTrack == glass::EffectiveArcTrackColor(theme)));
}

void ThemeTests::TheDeckColorIsReadDownThePage()
{
    // A notch cut into a panel's frame is filled with the deck at that height, so the frame
    // looks cut rather than painted over. On a gradient that is not the color at the top.
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);
    VERIFY_IS_TRUE(studio->Deck.Kind == glass::DeckKind::Gradient);

    VERIFY_IS_TRUE((glass::DeckColorAt(*studio, 0.0) == studio->Deck.Color));
    VERIFY_IS_TRUE((glass::DeckColorAt(*studio, 1.0) == studio->Deck.GradientEndColor));

    auto const middle = glass::DeckColorAt(*studio, 0.5);

    VERIFY_IS_LESS_THAN(glass::RelativeLuminance(middle), glass::RelativeLuminance(studio->Deck.Color));
    VERIFY_IS_GREATER_THAN(glass::RelativeLuminance(middle), glass::RelativeLuminance(studio->Deck.GradientEndColor));

    // Out of range is clamped rather than extrapolated past the deck's own ends.
    VERIFY_IS_TRUE((glass::DeckColorAt(*studio, -3.0) == studio->Deck.Color));
    VERIFY_IS_TRUE((glass::DeckColorAt(*studio, 7.0) == studio->Deck.GradientEndColor));

    // A flat deck is the same color all the way down.
    auto const* saw = glass::FindBuiltInTheme(L"Supersaw");
    VERIFY_IS_TRUE((glass::DeckColorAt(*saw, 0.8) == saw->Deck.Color));
}

// ============================================================================
// Five-iSH and Airy System, and the engine work they earned.
//
// Five-iSH prints its panel twice, so a name has to be inked for the surface it lands on. Airy
// System lights its knobs and faders all the time, keeps its buttons dark until they are on, and
// makes its pads colored plastic: three resting states on one page.
// ============================================================================

void ThemeTests::FiveIshPrintsTwoInksOnTwoSurfaces()
{
    auto const* five = glass::FindBuiltInTheme(L"Five-iSH");
    VERIFY_IS_NOT_NULL(five);

    VERIFY_IS_TRUE(five->PanelFill == glass::PanelFillStyle::Color);
    VERIFY_IS_TRUE(five->PanelColor.A != 0);
    VERIFY_IS_TRUE(five->InsetPanelColor.A != 0);
    VERIFY_IS_TRUE(five->SectionInkColor.A != 0);

    // Printed, not raised: neither layer casts a shadow.
    VERIFY_ARE_EQUAL(0, glass::EffectivePanelElevation(*five));

    auto const tan = glass::BlendOver(five->PanelColor, five->PanelEndColor, 0.5);
    auto const green = glass::BlendOver(five->InsetPanelColor, five->InsetPanelEndColor, 0.5);

    auto const sectionInkOnTan = glass::ContrastRatio(five->SectionInkColor, tan);
    auto const inkOnGreen = glass::ContrastRatio(five->InkColor, green);
    auto const inkOnMetal = glass::ContrastRatio(five->InkColor, five->Deck.GradientEndColor);

    Log::Comment(String().Format(L"Five-iSH: black on tan %.2f, white on green %.2f, white on metal %.2f",
        sectionInkOnTan, inkOnGreen, inkOnMetal));

    VERIFY_IS_GREATER_THAN_OR_EQUAL(sectionInkOnTan, 4.5);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(inkOnGreen, 4.5);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(inkOnMetal, 4.5);

    // Which is the reason for two: neither ink reads on the other's surface.
    VERIFY_IS_LESS_THAN(glass::ContrastRatio(five->InkColor, tan), 3.0);
    VERIFY_IS_LESS_THAN(glass::ContrastRatio(five->SectionInkColor, green), 4.5);

    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;
    knob.HueSlot = 0;

    auto const colors = glass::ResolveControlColors(knob, *five);

    VERIFY_IS_TRUE((colors.Label == five->InkColor));
    VERIFY_IS_TRUE((colors.SectionLabel == five->SectionInkColor));

    // The scales round a knob and beside a fader are printed in whichever ink the surface takes.
    VERIFY_ARE_EQUAL(five->SectionInkColor.R, colors.SectionKnobTick.R);
    VERIFY_ARE_EQUAL(five->SectionInkColor.R, colors.SectionFaderTick.R);
    VERIFY_ARE_EQUAL(five->InkColor.R, colors.KnobTick.R);
    VERIFY_ARE_EQUAL(five->InkColor.R, colors.FaderTick.R);

    VERIFY_ARE_EQUAL(
        static_cast<int32_t>(std::lround(255.0 * (five->FaderScalePercent / 100.0))),
        static_cast<int32_t>(colors.FaderTick.A));

    // and the rules printed on the metal are the print white, with square ends
    VERIFY_IS_TRUE((colors.Rule == five->RuleColor));
    VERIFY_IS_FALSE(five->RuleFades);
    VERIFY_ARE_EQUAL(five->SectionInkColor.R, colors.SectionRule.R);
    VERIFY_ARE_EQUAL(colors.Rule.A, colors.SectionRule.A);
}

void ThemeTests::FiveIshShowsColorOnlyInItsLamps()
{
    auto const* five = glass::FindBuiltInTheme(L"Five-iSH");
    VERIFY_IS_NOT_NULL(five);

    VERIFY_IS_TRUE(five->LampShape == glass::LampStyle::Dot);
    VERIFY_ARE_EQUAL(0, five->FillWhenOnPercent);
    VERIFY_IS_TRUE(five->PointerOnCap);
    VERIFY_IS_TRUE(five->KnobCapColor.A != 0);

    // So the picker has to say it cannot group knobs by color.
    VERIFY_IS_FALSE(five->CautionResourceKey.empty());

    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        glass::Control knob{};
        knob.Kind = glass::ControlKind::Knob;
        knob.HueSlot = slot;

        glass::Control fader{};
        fader.Kind = glass::ControlKind::Fader;
        fader.HueSlot = slot;

        glass::Control tab{};
        tab.Kind = glass::ControlKind::Toggle;
        tab.HueSlot = slot;

        auto const knobColors = glass::ResolveControlColors(knob, *five);
        auto const faderColors = glass::ResolveControlColors(fader, *five);
        auto const tabColors = glass::ResolveControlColors(tab, *five);

        auto const& hue = five->HueSlots[static_cast<size_t>(slot)];

        // Every value is the print white, whatever slot the control is on...
        VERIFY_IS_TRUE((knobColors.Pipe == five->ValueColor));
        VERIFY_ARE_EQUAL(five->ValueColor.R, faderColors.Fill.R);
        VERIFY_IS_TRUE((knobColors.Pointer == five->PointerColor));

        // ...and the slot lights the lamp and leaves the cap black.
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, tabColors.OnPlate.A);
        VERIFY_IS_TRUE((tabColors.Lamp == hue));
        VERIFY_IS_TRUE((tabColors.Plate == five->PlateColor));

        auto const cap = glass::BlendOver(five->PlateColor, five->PlateEndColor, 0.5);
        VERIFY_IS_GREATER_THAN_OR_EQUAL(glass::ContrastRatio(hue, cap), glass::MinimumSlotContrast);
    }

    // The line is on the cap because on the silver it would all but disappear.
    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;

    auto const colors = glass::ResolveControlColors(knob, *five);

    auto const onCap = glass::ContrastRatio(colors.Pointer, glass::BlendOver(colors.KnobCap, colors.KnobCapEnd, 0.5));
    auto const onSkirt = glass::ContrastRatio(colors.Pointer, glass::BlendOver(colors.KnobFace, colors.KnobFaceEnd, 0.5));

    Log::Comment(String().Format(L"Five-iSH pointer: %.2f on the cap, %.2f on the skirt", onCap, onSkirt));

    VERIFY_IS_GREATER_THAN_OR_EQUAL(onCap, glass::MinimumSlotContrast);
    VERIFY_IS_LESS_THAN(onSkirt, glass::MinimumSlotContrast);
}

void ThemeTests::ANeutralCapIsTheNeutralOnlyWhereTheThemeAsks()
{
    auto const* five = glass::FindBuiltInTheme(L"Five-iSH");
    VERIFY_IS_NOT_NULL(five);
    VERIFY_IS_TRUE(five->NeutralCaps);
    VERIFY_IS_TRUE(glass::HasNeutralColor(*five));

    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;
    tab.HueSlot = glass::NeutralSlot;

    glass::Control fader{};
    fader.Kind = glass::ControlKind::Fader;
    fader.HueSlot = glass::NeutralSlot;

    auto const tabColors = glass::ResolveControlColors(tab, *five);
    auto const faderColors = glass::ResolveControlColors(fader, *five);

    // A cream cap on a panel of black ones.
    VERIFY_IS_FALSE((tabColors.Plate == five->PlateColor));
    VERIFY_IS_GREATER_THAN(glass::RelativeLuminance(tabColors.Plate), glass::RelativeLuminance(five->NeutralColor));
    VERIFY_IS_LESS_THAN(glass::RelativeLuminance(tabColors.PlateEnd), glass::RelativeLuminance(five->NeutralColor));

    VERIFY_IS_GREATER_THAN(glass::RelativeLuminance(faderColors.Thumb), glass::RelativeLuminance(five->ThumbColor));

    // Its line is dark enough to read on the cream, and its lamp is a lamp color rather than
    // cream on cream.
    auto const capMiddle = glass::BlendOver(faderColors.Thumb, faderColors.ThumbEnd, 0.5);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(glass::ContrastRatio(faderColors.ThumbLine, capMiddle), glass::MinimumSlotContrast);
    VERIFY_IS_TRUE((tabColors.Lamp == five->HueSlots[0]));

    // Its name reads on it.
    auto const tabMiddle = glass::BlendOver(tabColors.Plate, tabColors.PlateEnd, 0.5);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(glass::ContrastRatio(tabColors.SwitchInk, tabMiddle), 4.5);

    // Any other slot keeps the black cap.
    tab.HueSlot = 2;
    VERIFY_IS_TRUE((glass::ResolveControlColors(tab, *five).Plate == five->PlateColor));

    // and a theme with a neutral that did not ask for cream caps leaves its caps alone
    auto const* saw = glass::FindBuiltInTheme(L"Supersaw");
    VERIFY_IS_NOT_NULL(saw);
    VERIFY_IS_TRUE(glass::HasNeutralColor(*saw));
    VERIFY_IS_FALSE(saw->NeutralCaps);

    tab.HueSlot = glass::NeutralSlot;
    VERIFY_IS_TRUE((glass::ResolveControlColors(tab, *saw).Plate == saw->PlateColor));
}

void ThemeTests::AirySwitchesRestDarkWhileKnobsAndFadersStayLit()
{
    auto const* airy = glass::FindBuiltInTheme(L"Airy System");
    VERIFY_IS_NOT_NULL(airy);

    auto const resolve = [airy](glass::ControlKind kind, int32_t slot)
        {
            glass::Control control{};
            control.Kind = kind;
            control.HueSlot = slot;

            return glass::ResolveControlColors(control, *airy);
        };

    auto const litRim = static_cast<uint8_t>(std::lround(255.0 * (airy->RimStrengthPercent / 100.0)));

    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        // What turns or slides is lit all the time...
        for (auto const kind : { glass::ControlKind::Knob, glass::ControlKind::Fader })
        {
            auto const colors = resolve(kind, slot);

            VERIFY_ARE_EQUAL(litRim, colors.Rim.A);
            VERIFY_ARE_EQUAL(airy->RestingGlowPercent / 100.0, colors.RestingGlow);
        }

        // ...and what is pressed rests dark.
        for (auto const kind : { glass::ControlKind::Button, glass::ControlKind::Toggle, glass::ControlKind::Pad })
        {
            auto const colors = resolve(kind, slot);

            VERIFY_ARE_EQUAL(uint8_t{ 0 }, colors.Rim.A);
            VERIFY_ARE_EQUAL(0.0, colors.RestingGlow);
        }

        // A glow never carries the state on its own: a lit button's edge arrives from nothing, and
        // the edge clears the graphics bar against the cap it runs round.
        auto const tab = resolve(glass::ControlKind::Toggle, slot);
        auto const edge = glass::BlendOver(airy->PlateColor, tab.OnRim, 1.0);

        VERIFY_IS_GREATER_THAN_OR_EQUAL(glass::ContrastRatio(edge, airy->PlateColor), glass::MinimumSlotContrast);
    }

    // A theme that does not split them rests its switches exactly like its knobs.
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);

    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;

    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;

    auto const knobColors = glass::ResolveControlColors(knob, *studio);
    auto const tabColors = glass::ResolveControlColors(tab, *studio);

    VERIFY_ARE_EQUAL(knobColors.Rim.A, tabColors.Rim.A);
    VERIFY_ARE_EQUAL(knobColors.RestingGlow, tabColors.RestingGlow);
}

void ThemeTests::AiryPadsAreColoredPlasticThatReadsLitOrNot()
{
    auto const* airy = glass::FindBuiltInTheme(L"Airy System");
    VERIFY_IS_NOT_NULL(airy);

    // Every slot and the white steps.
    std::vector<int32_t> slots{ glass::NeutralSlot };

    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        slots.push_back(slot);
    }

    for (auto const slot : slots)
    {
        glass::Control pad{};
        pad.Kind = glass::ControlKind::Pad;
        pad.HueSlot = slot;

        auto const hue = glass::ResolveHue(pad, *airy);
        auto const colors = glass::ResolveControlColors(pad, *airy);

        // Dim plastic at rest...
        auto expected = glass::BlendOver(airy->Deck.Color, hue, airy->PadFillAtRest);
        expected.A = 255;

        VERIFY_IS_TRUE((colors.Plate == expected));

        // ...and its color outright when lit.
        VERIFY_IS_TRUE((colors.OnPlate == hue));
        VERIFY_IS_TRUE((colors.OnPlateEnd == hue));

        auto const atRest = glass::ContrastRatio(colors.SwitchInk, colors.Plate);
        auto const lit = glass::ContrastRatio(colors.SwitchInkOn, hue);

        Log::Comment(String().Format(L"Airy pad slot %d: name %.2f at rest, %.2f lit", slot, atRest, lit));

        VERIFY_IS_GREATER_THAN_OR_EQUAL(atRest, 4.5);
        VERIFY_IS_GREATER_THAN_OR_EQUAL(lit, 4.5);
    }

    // A button beside it is still black at rest.
    glass::Control button{};
    button.Kind = glass::ControlKind::Button;
    button.HueSlot = 1;

    VERIFY_IS_TRUE((glass::ResolveControlColors(button, *airy).Plate == airy->PlateColor));

    // and on a theme with no pad family, a pad is a switch like any other
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);

    glass::Control pad{};
    pad.Kind = glass::ControlKind::Pad;

    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;

    VERIFY_IS_TRUE((glass::ResolveControlColors(pad, *studio).Plate == glass::ResolveControlColors(tab, *studio).Plate));
    VERIFY_IS_TRUE((glass::ResolveControlColors(pad, *studio).OnPlate == glass::ResolveControlColors(tab, *studio).OnPlate));
}

void ThemeTests::AnAirySliderLightsTheFrameAroundItsSlot()
{
    auto const* airy = glass::FindBuiltInTheme(L"Airy System");
    VERIFY_IS_NOT_NULL(airy);

    // The frame and its glow go around the slot, the way the instrument lights them.
    VERIFY_IS_TRUE(airy->FaderPlate == glass::FaderPlateStyle::Frame);

    glass::Control fader{};
    fader.Kind = glass::ControlKind::Fader;

    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        fader.HueSlot = slot;

        auto const colors = glass::ResolveControlColors(fader, *airy);

        // Which is what lets the cap be black: it reads against the light it crosses.
        auto const cap = glass::BlendOver(colors.Thumb, colors.ThumbEnd, 0.5);
        auto const frame = glass::BlendOver(airy->PlateColor, colors.Rim, 1.0);

        auto const ratio = glass::ContrastRatio(cap, frame);

        Log::Comment(String().Format(L"Airy slot %d: black cap on its lit frame %.2f : 1", slot + 1, ratio));

        VERIFY_IS_GREATER_THAN_OR_EQUAL(ratio, glass::MinimumSlotContrast);
        VERIFY_IS_GREATER_THAN_OR_EQUAL(glass::ContrastRatio(colors.ThumbLine, cap), 4.5);
    }

    // Nothing drawn before it moved its frame.
    for (auto const* name : { L"Studio Dark", L"Supersaw", L"Jove", L"Five-iSH" })
    {
        VERIFY_IS_FALSE(glass::FindBuiltInTheme(name)->FaderPlate == glass::FaderPlateStyle::Frame);
    }
}

void ThemeTests::AKnobRingCanBeItsOwnColor()
{
    auto const* airy = glass::FindBuiltInTheme(L"Airy System");
    VERIFY_IS_NOT_NULL(airy);
    VERIFY_IS_TRUE(airy->ArcGlow);

    for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
    {
        glass::Control knob{};
        knob.Kind = glass::ControlKind::Knob;
        knob.HueSlot = slot;

        auto const ring = glass::ResolveControlColors(knob, *airy).ArcTrack;
        auto const& hue = airy->HueSlots[static_cast<size_t>(slot)];

        // A blue knob sits in a blue ring rather than in everybody's gray one.
        VERIFY_ARE_EQUAL(hue.R, ring.R);
        VERIFY_ARE_EQUAL(hue.G, ring.G);
        VERIFY_ARE_EQUAL(hue.B, ring.B);
        VERIFY_ARE_EQUAL(
            static_cast<int32_t>(std::lround(255.0 * (airy->ArcTrackHuePercent / 100.0))),
            static_cast<int32_t>(ring.A));
    }

    // Every other theme keeps the one track it always had.
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);
    VERIFY_IS_FALSE(studio->ArcGlow);

    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;
    knob.HueSlot = 4;

    VERIFY_IS_TRUE((glass::ResolveControlColors(knob, *studio).ArcTrack == glass::EffectiveArcTrackColor(*studio)));
}

void ThemeTests::ARimOfZeroIsNoRim()
{
    // It used to fall back to a quarter, so a slider taken to the bottom still drew a rim.
    auto theme = *glass::FindBuiltInTheme(L"Studio Dark");

    glass::Control knob{};
    knob.Kind = glass::ControlKind::Knob;

    glass::Control tab{};
    tab.Kind = glass::ControlKind::Toggle;

    theme.RimStrengthPercent = 0;

    auto const none = glass::ResolveControlColors(knob, theme);

    VERIFY_ARE_EQUAL(uint8_t{ 0 }, none.Rim.A);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, none.TouchRim.A);

    // and the switch side can be zero on its own
    theme.RimStrengthPercent = 28;
    theme.SwitchRimStrengthPercent = 0;

    VERIFY_IS_GREATER_THAN(static_cast<int32_t>(glass::ResolveControlColors(knob, theme).Rim.A), 0);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, glass::ResolveControlColors(tab, theme).Rim.A);
}

void ThemeTests::ASectionIsRaisedLikeAControlUnlessTheThemeSaysOtherwise()
{
    glass::Theme theme{};

    // Minus one follows the plate, which is what every section did before this existed.
    VERIFY_ARE_EQUAL(theme.PlateElevation, glass::EffectivePanelElevation(theme));

    theme.PanelElevation = 0;
    VERIFY_ARE_EQUAL(0, glass::EffectivePanelElevation(theme));

    theme.PanelElevation = 40;
    VERIFY_ARE_EQUAL(40, glass::EffectivePanelElevation(theme));

    // Out of range from a hand-written file is held to the range rather than trusted.
    theme.PanelElevation = 250;
    VERIFY_ARE_EQUAL(100, glass::EffectivePanelElevation(theme));

    theme.PanelElevation = -1;
    theme.PlateElevation = 180;
    VERIFY_ARE_EQUAL(100, glass::EffectivePanelElevation(theme));
}

void ThemeTests::ARuleIsTheInkTurnedDownUnlessTheThemeNamesOne()
{
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);

    glass::Control line{};
    line.Kind = glass::ControlKind::Line;

    auto const colors = glass::ResolveControlColors(line, *studio);

    // The ink at a sixth, the design sheet's own divider.
    VERIFY_ARE_EQUAL(colors.Label.R, colors.Rule.R);
    VERIFY_ARE_EQUAL(colors.Label.G, colors.Rule.G);
    VERIFY_ARE_EQUAL(colors.Label.B, colors.Rule.B);
    VERIFY_ARE_EQUAL(static_cast<int32_t>(std::lround(colors.Label.A * 0.16)), static_cast<int32_t>(colors.Rule.A));
    VERIFY_IS_TRUE(studio->RuleFades);

    // A line is print. It has no rim to rest at and nothing to glow with.
    auto const* airy = glass::FindBuiltInTheme(L"Airy System");
    VERIFY_IS_NOT_NULL(airy);

    auto const airyLine = glass::ResolveControlColors(line, *airy);

    VERIFY_IS_TRUE((airyLine.Rule == airy->RuleColor));
    VERIFY_IS_FALSE(airy->RuleFades);
}

namespace
{
    // What a pad sits on: the control's plate over the deck, the way the renderer lays it.
    glass::ThemeColor PadBehind(glass::Control const& control, glass::Theme const& theme)
    {
        return glass::BlendOver(theme.Deck.Color, glass::ResolveControlColors(control, theme).Plate, 1.0);
    }

    int32_t LargestChannelGap(glass::ThemeColor const& a, glass::ThemeColor const& b)
    {
        return std::max({
            std::abs(static_cast<int32_t>(a.R) - static_cast<int32_t>(b.R)),
            std::abs(static_cast<int32_t>(a.G) - static_cast<int32_t>(b.G)),
            std::abs(static_cast<int32_t>(a.B) - static_cast<int32_t>(b.B)) });
    }
}

void ThemeTests::APadNameReadsOnEveryThemeLitOrNot()
{
    int32_t unreadable = 0;

    for (auto const& theme : glass::BuiltInThemes())
    {
        for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
        {
            glass::Control control{};
            control.Kind = glass::ControlKind::NotePads;
            control.HueSlot = slot;

            auto const behind = PadBehind(control, theme);
            auto const colors = glass::ResolvePadColors(control, theme, behind);

            for (size_t role = 0; role < 3; ++role)
            {
                auto const resting = glass::BlendOver(behind, colors.RestFill[role], 1.0);

                auto const atRest = glass::ContrastRatio(colors.RestInk[role], resting);
                auto const lit = glass::ContrastRatio(colors.LitInk[role], colors.LitFill[role]);
                auto const pressGap = LargestChannelGap(colors.LitFill[role], resting);

                // Every theme and slot is measured before failing, so one run shows all of them.
                if (atRest < 4.5 || lit < 4.5 || pressGap < 24)
                {
                    Log::Comment(String().Format(L"%s, slot %d, role %d: %.2f at rest, %.2f lit, press moves %d",
                        theme.Name.c_str(), slot, static_cast<int32_t>(role), atRest, lit, pressGap));

                    ++unreadable;
                }

                // Lit is opaque so a held pad looks the same over any plate.
                VERIFY_ARE_EQUAL(uint8_t{ 255 }, colors.LitFill[role].A);

                // And so is a resting pad: it stands up off the plate and casts a shadow on it.
                VERIFY_ARE_EQUAL(uint8_t{ 255 }, colors.RestFill[role].A);
            }
        }
    }

    VERIFY_ARE_EQUAL(0, unreadable);
}

void ThemeTests::TheRootTheKeyAndTheRestAreThreeDifferentPads()
{
    // The colors are how a player finds the key without hunting for it, so two roles that come
    // out looking alike on one theme are a grid that cannot be played on that theme.
    constexpr int32_t distinguishable = 24;

    int32_t alike = 0;

    for (auto const& theme : glass::BuiltInThemes())
    {
        for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
        {
            glass::Control control{};
            control.Kind = glass::ControlKind::HexPads;
            control.HueSlot = slot;

            auto const behind = PadBehind(control, theme);
            auto const colors = glass::ResolvePadColors(control, theme, behind);

            auto const outOfKey = glass::BlendOver(behind, colors.RestFill[static_cast<size_t>(glass::PadRole::OutOfKey)], 1.0);
            auto const inKey = glass::BlendOver(behind, colors.RestFill[static_cast<size_t>(glass::PadRole::InKey)], 1.0);
            auto const root = glass::BlendOver(behind, colors.RestFill[static_cast<size_t>(glass::PadRole::Root)], 1.0);

            auto const outToIn = LargestChannelGap(outOfKey, inKey);
            auto const inToRoot = LargestChannelGap(inKey, root);
            auto const outToRoot = LargestChannelGap(outOfKey, root);

            if (outToIn < distinguishable || inToRoot < distinguishable || outToRoot < distinguishable)
            {
                Log::Comment(String().Format(L"%s, slot %d: %d, %d, %d", theme.Name.c_str(), slot, outToIn, inToRoot, outToRoot));

                ++alike;
            }
        }
    }

    VERIFY_ARE_EQUAL(0, alike);
}

void ThemeTests::APadColorTypedInWinsAndABadOneFallsBack()
{
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");
    VERIFY_IS_NOT_NULL(studio);

    glass::Control control{};
    control.Kind = glass::ControlKind::NotePads;
    control.HueSlot = 1;
    control.Pads.RootColor = L"#FF0000";
    control.Pads.PressedColor = L"#00FF00";

    auto const behind = PadBehind(control, *studio);
    auto const typed = glass::ResolvePadColors(control, *studio, behind);

    auto const& rootRim = typed.RestRim[static_cast<size_t>(glass::PadRole::Root)];

    VERIFY_ARE_EQUAL(uint8_t{ 255 }, rootRim.R);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, rootRim.G);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, rootRim.B);

    for (auto const& lit : typed.LitFill)
    {
        VERIFY_IS_TRUE((lit == glass::ThemeColor{ 0, 255, 0, 255 }));
    }

    // Something that is not a color is the palette again, not black. Black on a dark deck is a
    // root nobody can find.
    control.Pads.RootColor = L"not a color";
    control.Pads.PressedColor.clear();

    auto const fallback = glass::ResolvePadColors(control, *studio, behind);

    auto const& across = studio->HueSlots[static_cast<size_t>((1 + glass::ThemeHueSlotCount / 2) % glass::ThemeHueSlotCount)];
    auto const& fallbackRim = fallback.RestRim[static_cast<size_t>(glass::PadRole::Root)];

    VERIFY_ARE_EQUAL(across.R, fallbackRim.R);
    VERIFY_ARE_EQUAL(across.G, fallbackRim.G);
    VERIFY_ARE_EQUAL(across.B, fallbackRim.B);

    // And with nothing typed, a held pad is its own color lifted, so it lights rather than
    // turning into some other color.
    auto const& inKeyRest = fallback.RestRim[static_cast<size_t>(glass::PadRole::InKey)];
    auto const& inKeyLit = fallback.LitFill[static_cast<size_t>(glass::PadRole::InKey)];

    VERIFY_IS_GREATER_THAN_OR_EQUAL(inKeyLit.R, inKeyRest.R);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(inKeyLit.G, inKeyRest.G);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(inKeyLit.B, inKeyRest.B);
}

void ThemeTests::ATypedPadColorIsTheColorOfThePadOnEveryTheme()
{
    // The three colors of the pads Pete sent: white outside the key, blue in it, pink roots.
    glass::ThemeColor const typed[3]{ { 255, 255, 255, 255 }, { 0x3D, 0x8B, 0xFF, 255 }, { 0xFF, 0x4F, 0xA3, 255 } };

    int32_t wrong = 0;

    for (auto const& theme : glass::BuiltInThemes())
    {
        glass::Control control{};
        control.Kind = glass::ControlKind::NotePads;
        control.Pads.OutOfKeyColor = L"#FFFFFF";
        control.Pads.InKeyColor = L"#3D8BFF";
        control.Pads.RootColor = L"#FF4FA3";

        auto const behind = PadBehind(control, theme);
        auto const colors = glass::ResolvePadColors(control, theme, behind);

        for (size_t role = 0; role < 3; ++role)
        {
            auto const resting = glass::BlendOver(behind, colors.RestFill[role], 1.0);
            auto const edge = glass::BlendOver(behind, colors.RestRim[role], 1.0);

            auto const asTyped = resting == typed[role];
            auto const findable = LargestChannelGap(resting, behind) >= 24 || LargestChannelGap(edge, behind) >= 24;
            auto const pressGap = LargestChannelGap(colors.LitFill[role], resting);
            auto const atRest = glass::ContrastRatio(colors.RestInk[role], resting);
            auto const lit = glass::ContrastRatio(colors.LitInk[role], colors.LitFill[role]);

            if (!asTyped || !findable || pressGap < 24 || atRest < 4.5 || lit < 4.5)
            {
                Log::Comment(String().Format(L"%s, role %d: typed %d, findable %d, press moves %d, %.2f at rest, %.2f lit",
                    theme.Name.c_str(), static_cast<int32_t>(role), asTyped, findable, pressGap, atRest, lit));

                ++wrong;
            }
        }
    }

    VERIFY_ARE_EQUAL(0, wrong);
}

void ThemeTests::PadsAreRimmedTheWayTheThemeRimsItsControls()
{
    // A pad on the grid is a small version of the theme's own pad control, so Supersaw's rimless
    // black molding gives rimless pads and Studio Dark's hairline gives every pad one.
    auto const* supersaw = glass::FindBuiltInTheme(L"Supersaw");
    auto const* studio = glass::FindBuiltInTheme(L"Studio Dark");

    VERIFY_IS_NOT_NULL(supersaw);
    VERIFY_IS_NOT_NULL(studio);

    glass::Control control{};
    control.Kind = glass::ControlKind::NotePads;

    VERIFY_ARE_EQUAL(uint8_t{ 0 }, glass::ResolveControlColors(control, *supersaw).Rim.A);

    auto const bare = glass::ResolvePadColors(control, *supersaw, PadBehind(control, *supersaw));
    auto const lined = glass::ResolvePadColors(control, *studio, PadBehind(control, *studio));

    for (size_t role = 0; role < 3; ++role)
    {
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, bare.RestRim[role].A);
        VERIFY_ARE_NOT_EQUAL(uint8_t{ 0 }, lined.RestRim[role].A);
    }
}
