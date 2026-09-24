// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThemeFileTests.h"

#include "ThemeStore.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

void ThemeFileTests::WritesColorsThePeopleCanRead()
{
    // A theme is a file people hand-edit and paste colors into, so a slot is "#FFC247" rather
    // than four numbers on four lines.
    VERIFY_ARE_EQUAL(std::wstring{ L"#FFC247" }, glass::ColorToText({ 0xFF, 0xC2, 0x47, 255 }));
    VERIFY_ARE_EQUAL(std::wstring{ L"#000000" }, glass::ColorToText({ 0, 0, 0, 255 }));

    // Alpha only appears when it is not full, so the common case stays short.
    VERIFY_ARE_EQUAL(std::wstring{ L"#80FFC247" }, glass::ColorToText({ 0xFF, 0xC2, 0x47, 0x80 }));
}

void ThemeFileTests::ParsesBothColorForms()
{
    glass::ThemeColor color{};

    VERIFY_IS_TRUE(glass::TryParseColor(L"#FFC247", color));
    VERIFY_IS_TRUE((color == glass::ThemeColor{ 0xFF, 0xC2, 0x47, 255 }));

    VERIFY_IS_TRUE(glass::TryParseColor(L"#80FFC247", color));
    VERIFY_IS_TRUE((color == glass::ThemeColor{ 0xFF, 0xC2, 0x47, 0x80 }));

    // lower case is what a person actually types
    VERIFY_IS_TRUE(glass::TryParseColor(L"#ffc247", color));
    VERIFY_IS_TRUE((color == glass::ThemeColor{ 0xFF, 0xC2, 0x47, 255 }));
}

void ThemeFileTests::RejectsColorsThatAreNotColors()
{
    glass::ThemeColor color{};

    VERIFY_IS_FALSE(glass::TryParseColor(L"", color));
    VERIFY_IS_FALSE(glass::TryParseColor(L"FFC247", color));          // no hash
    VERIFY_IS_FALSE(glass::TryParseColor(L"#FFC24", color));          // five digits
    VERIFY_IS_FALSE(glass::TryParseColor(L"#FFC2477", color));        // seven digits
    VERIFY_IS_FALSE(glass::TryParseColor(L"#GGGGGG", color));         // not hex
    VERIFY_IS_FALSE(glass::TryParseColor(L"#FFC24 7", color));        // a space in the middle
    VERIFY_IS_FALSE(glass::TryParseColor(L"red", color));
}

void ThemeFileTests::EveryShippedThemeSurvivesARoundTrip()
{
    for (auto const& theme : glass::BuiltInThemes())
    {
        auto const text = glass::WriteThemeToJson(theme);
        VERIFY_IS_FALSE(text.empty());

        auto const read = glass::ReadThemeFromJson(text);
        VERIFY_IS_TRUE(read.Succeeded);

        VERIFY_ARE_EQUAL(theme.Name, read.Value.Name);
        VERIFY_ARE_EQUAL(theme.CornerRadius, read.Value.CornerRadius);
        VERIFY_ARE_EQUAL(theme.GlassTintPercent, read.Value.GlassTintPercent);
        VERIFY_ARE_EQUAL(theme.GlowStrength, read.Value.GlowStrength);
        VERIFY_ARE_EQUAL(theme.FillAtRest, read.Value.FillAtRest);
        VERIFY_ARE_EQUAL(theme.LampCount, read.Value.LampCount);
        VERIFY_ARE_EQUAL(theme.MinimumLampRingSize, read.Value.MinimumLampRingSize);

        VERIFY_IS_TRUE(theme.Deck.Color == read.Value.Deck.Color);
        VERIFY_IS_TRUE(theme.TrackColor == read.Value.TrackColor);
        VERIFY_IS_TRUE(theme.NeutralRimColor == read.Value.NeutralRimColor);

        VERIFY_IS_TRUE(theme.Labels == read.Value.Labels);
        VERIFY_IS_TRUE(theme.Rim == read.Value.Rim);
        VERIFY_IS_TRUE(theme.ValueStrip == read.Value.ValueStrip);
        VERIFY_IS_TRUE(theme.ValueIndicator == read.Value.ValueIndicator);

        for (int32_t i = 0; i < glass::ThemeHueSlotCount; ++i)
        {
            VERIFY_IS_TRUE(theme.HueSlots[static_cast<size_t>(i)] == read.Value.HueSlots[static_cast<size_t>(i)]);
        }
    }
}

void ThemeFileTests::WritingTheSameThemeTwiceProducesTheSameBytes()
{
    auto const& theme = *glass::FindBuiltInTheme(L"Bigwig");

    auto const first = glass::WriteThemeToJson(theme);
    auto const second = glass::WriteThemeToJson(theme);

    VERIFY_ARE_EQUAL(first, second);

    // and a trip through the reader does not change it either
    auto const read = glass::ReadThemeFromJson(first);
    VERIFY_IS_TRUE(read.Succeeded);

    auto written = read.Value;
    written.Name = theme.Name;

    VERIFY_ARE_EQUAL(first, glass::WriteThemeToJson(written));
}

void ThemeFileTests::ReadsAHandAuthoredTheme()
{
    auto const json = LR"JSON({
        "name": "Hand Rolled",
        "hueSlots": [ "#FF0000", "#00FF00", "#0000FF", "#FFFF00", "#FF00FF", "#00FFFF" ],
        "deck": { "kind": "gradient", "color": "#101010", "gradientEndColor": "#303030" },
        "cornerRadius": 12,
        "glassTintPercent": 50,
        "labels": "inside",
        "rim": "neutralEdge",
        "valueStrip": "top",
        "valueIndicator": "segmentedLamps",
        "lampCount": 32
    })JSON";

    auto const read = glass::ReadThemeFromJson(json);
    VERIFY_IS_TRUE(read.Succeeded);

    VERIFY_ARE_EQUAL(std::wstring{ L"Hand Rolled" }, read.Value.Name);
    VERIFY_IS_TRUE((read.Value.HueSlots[0] == glass::ThemeColor{ 255, 0, 0, 255 }));
    VERIFY_IS_TRUE((read.Value.HueSlots[5] == glass::ThemeColor{ 0, 255, 255, 255 }));
    VERIFY_IS_TRUE(read.Value.Deck.Kind == glass::DeckKind::Gradient);
    VERIFY_IS_TRUE((read.Value.Deck.GradientEndColor == glass::ThemeColor{ 0x30, 0x30, 0x30, 255 }));
    VERIFY_ARE_EQUAL(12, read.Value.CornerRadius);
    VERIFY_ARE_EQUAL(50, read.Value.GlassTintPercent);
    VERIFY_IS_TRUE(read.Value.Labels == glass::LabelPlacement::Inside);
    VERIFY_IS_TRUE(read.Value.Rim == glass::RimSource::NeutralEdge);
    VERIFY_ARE_EQUAL(32, read.Value.LampCount);
}

void ThemeFileTests::AHalfWrittenThemeIsStillUsable()
{
    // Somebody starts a theme, sets two slots and saves. Six black slots on a black deck would
    // be a theme that cannot be seen and cannot be debugged.
    auto const read = glass::ReadThemeFromJson(LR"JSON({
        "name": "Barely Started",
        "hueSlots": [ "#FF0000", "#00FF00" ]
    })JSON");

    VERIFY_IS_TRUE(read.Succeeded);

    VERIFY_IS_TRUE((read.Value.HueSlots[0] == glass::ThemeColor{ 255, 0, 0, 255 }));
    VERIFY_IS_TRUE((read.Value.HueSlots[1] == glass::ThemeColor{ 0, 255, 0, 255 }));

    auto const& studio = *glass::FindBuiltInTheme(L"Studio Dark");

    for (int32_t i = 2; i < glass::ThemeHueSlotCount; ++i)
    {
        VERIFY_IS_TRUE(read.Value.HueSlots[static_cast<size_t>(i)] == studio.HueSlots[static_cast<size_t>(i)]);
    }

    VERIFY_IS_TRUE(read.Value.Deck.Color == studio.Deck.Color);

    // and the result is legible rather than merely present
    for (auto const& slot : glass::MeasureContrast(read.Value))
    {
        VERIFY_IS_TRUE(slot.MeetsMinimum);
    }
}

void ThemeFileTests::AThemeFileCannotClaimToBeBuiltIn()
{
    auto const read = glass::ReadThemeFromJson(LR"JSON({
        "name": "Studio Dark",
        "isBuiltIn": true
    })JSON");

    VERIFY_IS_TRUE(read.Succeeded);

    // A shared file that could mark itself built in would make itself unoverwritable on somebody
    // else's PC, and would shadow a shipped theme in the picker.
    VERIFY_IS_FALSE(read.Value.IsBuiltIn);

    // and saving it must not be allowed to destroy the shipped one
    VERIFY_IS_FALSE(glass::WriteThemeFile(read.Value, L"C:\\nowhere\\Studio Dark.miditheme.json"));
}

void ThemeFileTests::RefusesADeckImageThatEscapesItsFolder()
{
    wchar_t const* attempts[]
    {
        LR"JSON({ "name": "T", "deck": { "image": "..\\..\\Windows\\System32\\drivers\\etc\\hosts" } })JSON",
        LR"JSON({ "name": "T", "deck": { "image": "C:\\Windows\\System32\\config\\SAM" } })JSON",
        LR"JSON({ "name": "T", "deck": { "image": "sub/folder/picture.png" } })JSON",
        LR"JSON({ "name": "T", "deck": { "image": ".." } })JSON",
    };

    for (auto const* attempt : attempts)
    {
        auto const read = glass::ReadThemeFromJson(attempt);
        VERIFY_IS_TRUE(read.Succeeded);

        // A theme can arrive from a stranger. The deck image is a bare file name inside the
        // shared assets folder, never a path out of it.
        VERIFY_IS_TRUE(read.Value.Deck.ImageFileName.empty());
    }

    // a plain name is still accepted
    auto const good = glass::ReadThemeFromJson(LR"JSON({ "name": "T", "deck": { "image": "stage.jpg" } })JSON");
    VERIFY_ARE_EQUAL(std::wstring{ L"stage.jpg" }, good.Value.Deck.ImageFileName);
}

void ThemeFileTests::SurvivesAHostileThemeFile()
{
    auto const read = glass::ReadThemeFromJson(LR"JSON({
        "name": 12345,
        "hueSlots": "not an array",
        "deck": [ 1, 2, 3 ],
        "cornerRadius": -900,
        "glassTintPercent": 10000,
        "glowStrength": "bright",
        "fillAtRest": 42,
        "lampCount": 0,
        "minimumLampRingSize": -1,
        "labels": "somewhere",
        "valueIndicator": null
    })JSON");

    VERIFY_IS_TRUE(read.Succeeded);

    auto const& theme = read.Value;

    VERIFY_IS_TRUE(theme.CornerRadius >= 0);
    VERIFY_IS_TRUE(theme.GlassTintPercent >= 0 && theme.GlassTintPercent <= 100);
    VERIFY_IS_TRUE(theme.GlowStrength >= 0 && theme.GlowStrength <= 100);
    VERIFY_IS_TRUE(theme.FillAtRest >= 0.0 && theme.FillAtRest <= 1.0);
    VERIFY_IS_GREATER_THAN(theme.LampCount, 1);
    VERIFY_IS_GREATER_THAN(theme.MinimumLampRingSize, 0);

    VERIFY_IS_FALSE(glass::ReadThemeFromJson(L"not json at all").Succeeded);
    VERIFY_IS_FALSE(glass::ReadThemeFromJson(L"").Succeeded);
}
