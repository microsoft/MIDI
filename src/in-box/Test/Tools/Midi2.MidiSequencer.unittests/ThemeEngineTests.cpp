// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>
#undef GetObject

#include "ThemeEngineTests.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "ThemeColor.h"
#include "ThemeDefinition.h"
#include "ThemePalette.h"
#include "ThemePixels.h"
#include "ThemeResolver.h"
#include "ThemeStyle.h"
#include "ThemeSyntax.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace te = themeengine;

namespace
{
    class TestScope final : public te::ExpressionScope
    {
    public:
        std::map<std::wstring, te::Color, std::less<>> Colors{};
        std::map<std::wstring, double, std::less<>> Numbers{};

        std::optional<te::Color> NamedColor(std::wstring_view name) const noexcept override
        {
            auto const found = Colors.find(name);
            return found != Colors.end() ? std::optional<te::Color>{ found->second } : std::nullopt;
        }

        std::optional<double> NamedNumber(std::wstring_view name) const noexcept override
        {
            auto const found = Numbers.find(name);
            return found != Numbers.end() ? std::optional<double>{ found->second } : std::nullopt;
        }
    };

    te::Color Hex(uint32_t rgb, double alpha = 1.0)
    {
        return te::Color::FromRgb8(static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb), alpha);
    }

    void VerifyNear(double expected, double actual, double tolerance, std::wstring_view what)
    {
        auto const ok = std::abs(expected - actual) <= tolerance;

        if (!ok)
        {
            Log::Comment(String().Format(L"%s: expected %.6f, got %.6f", std::wstring{ what }.c_str(), expected, actual));
        }

        VERIFY_IS_TRUE(ok);
    }

    void VerifyColor(te::Color const& expected, te::Color const& actual, double tolerance, std::wstring_view what)
    {
        VerifyNear(expected.R, actual.R, tolerance, std::wstring{ what } + L" red");
        VerifyNear(expected.G, actual.G, tolerance, std::wstring{ what } + L" green");
        VerifyNear(expected.B, actual.B, tolerance, std::wstring{ what } + L" blue");
        VerifyNear(expected.A, actual.A, tolerance, std::wstring{ what } + L" alpha");
    }

    te::Color Evaluate(std::wstring_view text, te::ExpressionScope const& scope)
    {
        std::wstring error{};
        auto const parsed = te::ColorExpression::Parse(text, error);

        if (!parsed.has_value())
        {
            Log::Comment(String().Format(L"\"%s\" didn't parse: %s", std::wstring{ text }.c_str(), error.c_str()));
        }

        VERIFY_IS_TRUE(parsed.has_value());
        return parsed.has_value() ? parsed->Evaluate(scope) : te::Color{};
    }

    te::Color Evaluate(std::wstring_view text)
    {
        TestScope const scope{};
        return Evaluate(text, scope);
    }

    std::filesystem::path ThemesFolder()
    {
        return std::filesystem::path{ __FILE__ }.parent_path() / L".." / L".." / L".." / L"user-tools" / L"midi-sequencer" / L"Themes";
    }

    std::wstring ReadUtf8File(std::filesystem::path const& path)
    {
        std::ifstream file{ path, std::ios::binary };
        std::stringstream buffer{};
        buffer << file.rdbuf();
        auto const bytes = buffer.str();

        if (bytes.empty())
        {
            return {};
        }

        auto const length = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        std::wstring text(static_cast<size_t>(length), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), text.data(), length);
        return text;
    }

    std::shared_ptr<te::ThemeDefinition const> LoadBuiltIn(std::wstring_view name)
    {
        auto const path = ThemesFolder() / (std::wstring{ name } + L".json");
        auto const result = te::LoadTheme(ReadUtf8File(path));

        for (auto const& problem : result.Problems)
        {
            Log::Comment(String().Format(L"%s: %s", std::wstring{ name }.c_str(), problem.c_str()));
        }

        VERIFY_IS_TRUE(result.Problems.empty());
        VERIFY_IS_NOT_NULL(result.Theme.get());
        return result.Theme;
    }

    bool HasProblemContaining(te::ThemeLoadResult const& result, std::wstring_view text)
    {
        for (auto const& problem : result.Problems)
        {
            if (problem.find(text) != std::wstring::npos)
            {
                return true;
            }
        }

        Log::Comment(String().Format(L"No problem mentioned \"%s\". Problems:", std::wstring{ text }.c_str()));

        for (auto const& problem : result.Problems)
        {
            Log::Comment(problem.c_str());
        }

        return false;
    }

    // A theme with only the parts a test needs.
    std::wstring MinimalTheme(std::wstring_view light, std::wstring_view dark = L"\"extends\": \"light\"", std::wstring_view extra = L"")
    {
        return std::wstring{ L"{ \"themeEngine\": 1, \"id\": \"test\", \"name\": \"Test\", " } + std::wstring{ extra } +
            L"\"light\": { " + std::wstring{ light } + L" }, \"dark\": { " + std::wstring{ dark } + L" } }";
    }
}

void ThemeEngineTests::ColorsConvertAsCssDoes()
{
    // Edge's own numbers for these colors, from the comps' computed styles.
    auto const warmGray = te::ToOklch(Hex(0x504C44));
    VerifyNear(0.417817, warmGray.L, 0.0002, L"#504C44 lightness");
    VerifyNear(0.0140039, warmGray.C, 0.0002, L"#504C44 chroma");
    VerifyNear(84.5165, warmGray.H, 0.1, L"#504C44 hue");

    auto const aqua = te::ToOklch(Hex(0x031E24));
    VerifyNear(0.0347322, aqua.C, 0.0002, L"#031E24 chroma");
    VerifyNear(214.855, aqua.H, 0.1, L"#031E24 hue");

    VerifyNear(332.909, te::ToOklch(Hex(0x33192F)).H, 0.1, L"#33192F hue");

    for (auto const rgb : { 0x000000u, 0xFFFFFFu, 0xFF8A65u, 0x031E24u, 0x33192Fu, 0x60CDFFu, 0x123456u })
    {
        auto const color = Hex(rgb);
        VerifyColor(color, te::FromOklch(te::ToOklch(color), 1.0), 0.000001, L"a round trip through OKLCH");
    }

    // The most chroma sRGB has at L 0.13 is narrowest near hue 200 (porcelain-tint.css).
    VerifyNear(0.0230, te::MaximumSrgbChroma(0.13, 215.0), 0.0005, L"sRGB's chroma at L 0.13, hue 215");
    VERIFY_IS_TRUE(te::IsInSrgbGamut(te::FromOklch({ 0.13, 0.021, 214.855 }, 1.0)));
    VERIFY_IS_FALSE(te::IsInSrgbGamut(te::FromOklch({ 0.13, 0.035, 214.855 }, 1.0)));

    VerifyNear(21.0, te::ContrastRatio(Hex(0xFFFFFF), Hex(0x000000)), 0.001, L"white on black");
    VerifyNear(4.54, te::ContrastRatio(Hex(0x767676), Hex(0xFFFFFF)), 0.01, L"#767676 on white");

    auto const over = te::Over(Hex(0x000000, 0.5), Hex(0xFFFFFF));
    VerifyColor(Hex(0x808080), over, 0.003, L"half black over white");
}

void ThemeEngineTests::ColorMixFollowsCss()
{
    // Edge: color-mix(in srgb, #FF8A65 26%, white) is color(srgb 1 0.880706 0.84298).
    VerifyColor({ 1.0, 0.880706, 0.84298, 1.0 }, Evaluate(L"color-mix(in srgb, #FF8A65 26%, white)"), 0.00001, L"a 26% mix");

    // Shares that add up to less than 100% leave the rest see-through.
    VerifyColor({ 0.8 / 0.9, 0, 0, 0.9 }, Evaluate(L"color-mix(in srgb, #FF0000 80%, black 10%)"), 0.00001, L"80% red and 10% black");

    // Mixing with transparent is premultiplied, so the color stays and only the alpha changes.
    VerifyColor({ 1, 0, 0, 0.5 }, Evaluate(L"color-mix(in srgb, #FF0000, transparent)"), 0.00001, L"red and transparent");

    // A share can come before its color, and one share leaves the other the rest.
    VerifyColor(Evaluate(L"color-mix(in srgb, #FF8A65 26%, white)"), Evaluate(L"color-mix(in srgb, 26% #FF8A65, white 74%)"), 0.00001, L"shares written the other way");

    // OKLCH goes the shorter way round the hue: red and blue meet in magenta, not green.
    auto const red = te::ToOklch(Hex(0xFF0000));
    auto const blue = te::ToOklch(Hex(0x0000FF));
    auto const mixed = te::ToOklch(Evaluate(L"color-mix(in oklch, #FF0000, #0000FF)"));
    VerifyNear(std::fmod((red.H + 360.0 + blue.H) / 2.0, 360.0), mixed.H, 0.5, L"the hue between red and blue");

    // A gray has no hue of its own, so it takes the other color's.
    auto const grayMix = te::ToOklch(Evaluate(L"color-mix(in oklch, #808080, #0000FF)"));
    VerifyNear(blue.H, grayMix.H, 0.5, L"gray and blue keep blue's hue");
}

void ThemeEngineTests::ColorsAreReadAsCssWritesThem()
{
    VerifyColor(Hex(0xAABBCC), Evaluate(L"#ABC"), 0.00001, L"#ABC");
    VerifyColor(Hex(0xAABBCC, 128.0 / 255.0), Evaluate(L"#AABBCC80"), 0.00001, L"#AABBCC80 is CSS order, alpha last");
    VerifyColor(Hex(0xFF0000, 0.5), Evaluate(L"rgb(255 0 0 / 50%)"), 0.00001, L"rgb() with a slash");
    VerifyColor(Hex(0x0A141E, 0.5), Evaluate(L"rgba(10, 20, 30, 0.5)"), 0.00001, L"rgba() with commas");
    VerifyColor(Hex(0xFF0000), Evaluate(L"rgb(100% 0% 0%)"), 0.00001, L"rgb() in percentages");
    VerifyColor({ 1.0, 0.880706, 0.84298, 0.25 }, Evaluate(L"color(srgb 1 0.880706 0.84298 / .25)"), 0.00001, L"color(srgb)");
    VerifyColor({ 0, 0, 0, 0 }, Evaluate(L"transparent"), 0.0, L"transparent");

    auto const lch = te::ToOklch(Evaluate(L"oklch(50% 25% 120deg)"));
    VerifyNear(0.5, lch.L, 0.0001, L"oklch() lightness as a percentage");
    VerifyNear(0.1, lch.C, 0.0001, L"oklch() chroma as a percentage");
    VerifyNear(120.0, lch.H, 0.01, L"oklch() hue in degrees");

    TestScope scope{};
    scope.Colors.emplace(L"face-pressed", Hex(0xDEDDD8));
    VerifyColor(Hex(0xDEDDD8, 0.5), Evaluate(L"rgb(from face-pressed r g b / .5)", scope), 0.00001, L"a theme's color by name");
    VerifyColor({ 0, 0, 0, 0 }, Evaluate(L"no-such-color", scope), 0.0, L"a missing name is transparent");

    std::wstring error{};
    auto const parsed = te::ColorExpression::Parse(L"color-mix(in srgb, track 22%, face-pressed)", error);
    VERIFY_IS_TRUE(parsed.has_value());
    VERIFY_IS_TRUE(parsed->RefersTo(L"track"));
    VERIFY_IS_TRUE(parsed->RefersTo(L"face-pressed"));
    VERIFY_IS_FALSE(parsed->RefersTo(L"white"));
}

void ThemeEngineTests::RelativeColorsTakeTheOriginsChannels()
{
    TestScope scope{};
    scope.Colors.emplace(L"tint", Hex(0x031E24));
    scope.Numbers.emplace(L"glaze", 1.0);

    // porcelain-tint.css: the shade is a deep shade of the tint, its chroma capped.
    auto const shade = te::ToOklch(Evaluate(L"oklch(from tint 0.42 min(c, 0.05) h)", scope));
    VerifyNear(0.42, shade.L, 0.0001, L"the shade's lightness");
    VerifyNear(0.0347322, shade.C, 0.0002, L"the shade keeps the tint's chroma under the cap");
    VerifyNear(214.855, shade.H, 0.1, L"the shade keeps the tint's hue");

    auto const surface = te::ToOklch(Evaluate(L"oklch(from tint 0.915 calc(min(c * 0.45, 0.012) * glaze) h)", scope));
    VerifyNear(0.915, surface.L, 0.0001, L"the glazed surface's lightness");
    VerifyNear(0.012, surface.C, 0.0001, L"the glazed surface is capped");

    scope.Numbers[L"glaze"] = 0.0;
    VerifyNear(0.0, te::ToOklch(Evaluate(L"oklch(from tint 0.915 calc(min(c * 0.45, 0.012) * glaze) h)", scope)).C, 0.0001, L"no glaze, no chroma");

    VerifyColor(Hex(0xFF8A65, 0.62), Evaluate(L"rgb(from #FF8A65 r g b / 0.62)"), 0.00001, L"rgb(from) keeps the channels");
    VerifyColor(Hex(0x504C44, 0.18), Evaluate(L"oklch(from #504C44 l c h / .18)"), 0.000001, L"oklch(from) keeps the channels");
    VerifyColor(Hex(0x7F7F7F), Evaluate(L"rgb(from #FEFEFE calc(r / 2) calc(g / 2) calc(b / 2))"), 0.00001, L"calc() on a channel");
    VerifyColor(Hex(0x203040), Evaluate(L"rgb(from #102030 calc(r + 16) clamp(0, g + 16, 255) max(b + 16, 0))"), 0.00001, L"sums and clamp()");
}

void ThemeEngineTests::BadColorsAreRefusedWithAReason()
{
    std::wstring deep(40, L'(');
    deep = L"oklch(calc" + deep + L"1" + std::wstring(40, L')') + L" 0 0)";

    for (auto const text : {
        L"", L"#12", L"#GGG", L"#12345", L"rgb(1 2)", L"oklch(0.5 0.1)", L"color-mix(srgb, red, blue)",
        L"color-mix(in hsl, red, blue)", L"foo(1)", L"oklch(from)", L"rgb(1 2 3", L"red blue", L"1px",
        L"rgb(1 2 3) junk", L"oklch(0.5 0.1 10 / )", L"color(display-p3 1 0 0)", L"rgb(1em 2 3)", L"$token",
        deep.c_str() })
    {
        std::wstring error{};
        auto const parsed = te::ColorExpression::Parse(text, error);

        if (parsed.has_value() || error.empty())
        {
            Log::Comment(String().Format(L"\"%s\" was accepted", text));
        }

        VERIFY_IS_FALSE(parsed.has_value());
        VERIFY_IS_FALSE(error.empty());
    }

    std::wstring error{};
    VERIFY_IS_FALSE(te::ColorExpression::Parse(std::wstring(5000, L'a'), error).has_value());
}

void ThemeEngineTests::StyleValuesAreReadAsCssWritesThem()
{
    std::wstring error{};

    std::vector<te::BackgroundLayer> layers{};
    VERIFY_IS_TRUE(te::ParseBackground(L"linear-gradient(145deg, #ECEBE7, #DDDCD8)", layers, error));
    VERIFY_ARE_EQUAL(1u, layers.size());
    VERIFY_IS_TRUE(layers[0].Kind == te::BackgroundKind::Linear);
    VerifyNear(145.0, layers[0].Angle, 0.0, L"the gradient's angle");
    VERIFY_ARE_EQUAL(2u, layers[0].Stops.size());

    VERIFY_IS_TRUE(te::ParseBackground(L"radial-gradient(circle at 35% 30%, #5A5A60, #2A2A2D 72%)", layers, error));
    VERIFY_IS_TRUE(layers[0].Kind == te::BackgroundKind::Radial);
    VERIFY_IS_TRUE(layers[0].Circle);
    VerifyNear(0.35, layers[0].CenterX, 0.0, L"the center across");
    VerifyNear(0.30, layers[0].CenterY, 0.0, L"the center down");
    VerifyNear(0.72, layers[0].Stops[1].Position.value_or(-1), 0.0, L"the last stop");

    VERIFY_IS_TRUE(te::ParseBackground(L"radial-gradient(130% 150% at 25% 15%, red, blue 75%), #F3F0E8", layers, error));
    VERIFY_ARE_EQUAL(2u, layers.size());
    VerifyNear(1.3, layers[0].SizeX.value_or(0), 0.0, L"the ellipse's width");
    VerifyNear(1.5, layers[0].SizeY.value_or(0), 0.0, L"the ellipse's height");
    VERIFY_IS_TRUE(layers[1].Kind == te::BackgroundKind::Solid);

    VERIFY_IS_TRUE(te::ParseBackground(L"none", layers, error));
    VERIFY_IS_TRUE(layers.empty());

    std::vector<te::ShadowLayer> shadows{};
    VERIFY_IS_TRUE(te::ParseShadows(L"3px 4px 11px oklch(0.42 0.01 85 / .18), -3px -3px 8px white", shadows, error));
    VERIFY_ARE_EQUAL(2u, shadows.size());
    VerifyNear(3.0, shadows[0].X, 0.0, L"x");
    VerifyNear(4.0, shadows[0].Y, 0.0, L"y");
    VerifyNear(11.0, shadows[0].Blur, 0.0, L"blur");
    VerifyNear(-3.0, shadows[1].X, 0.0, L"the light's x");

    VERIFY_IS_TRUE(te::ParseShadows(L"inset 0 1px 0 rgba(255, 255, 255, .35), rgba(0, 0, 0, .35) 0 2px 3px 1px", shadows, error));
    VERIFY_IS_TRUE(shadows[0].Inset);
    VERIFY_IS_FALSE(shadows[1].Inset);
    VerifyNear(1.0, shadows[1].Spread, 0.0, L"a spread after a color written first");

    VERIFY_IS_TRUE(te::ParseShadows(L"0 0 0 2px accent, clip-lift", shadows, error));
    VERIFY_ARE_EQUAL(std::wstring{ L"clip-lift" }, shadows[1].Reference);

    VERIFY_IS_FALSE(te::ParseShadows(L"3px red", shadows, error));
    VERIFY_IS_FALSE(te::ParseShadows(L"1px 2px 3px", shadows, error));

    te::Line line{};
    VERIFY_IS_TRUE(te::ParseLine(L"1px dashed rgba(60, 55, 45, .45)", line, error));
    VerifyNear(1.0, line.Width, 0.0, L"the line's width");
    VERIFY_IS_TRUE(line.Style == te::LineStyle::Dashed);

    te::Corners corners{};
    VERIFY_IS_TRUE(te::ParseCorners(L"4px 4px 0 0", corners, error));
    VerifyNear(4.0, corners[1].Value, 0.0, L"top right");
    VerifyNear(0.0, corners[2].Value, 0.0, L"bottom right");

    te::Edges edges{};
    VERIFY_IS_TRUE(te::ParseEdges(L"5px 4px", edges, error));
    VerifyNear(4.0, edges[3], 0.0, L"the left margin");

    te::FilterValue filter{};
    VERIFY_IS_TRUE(te::ParseFilter(L"saturate(.2) brightness(1.04)", filter, error));
    VerifyNear(0.2, filter.Saturate, 0.0, L"saturate");
    VerifyNear(1.04, filter.Brightness, 0.0, L"brightness");

    te::StyleSpec style{};
    VERIFY_IS_TRUE(te::SetStyleProperty(style, L"note-color", L"track", error));
    VERIFY_IS_TRUE(te::SetStyleProperty(style, L"caption-height", L"15px", error));
    VERIFY_IS_TRUE(te::SetStyleProperty(style, L"cap-shape", L"circle", error));
    VERIFY_IS_TRUE(te::SetStyleProperty(style, L"border-radius", L"50%", error));
    VERIFY_IS_TRUE(style.Colors.contains(L"note-color"));
    VerifyNear(15.0, style.Numbers[L"caption-height"], 0.0, L"a number of the part's own");
    VERIFY_ARE_EQUAL(std::wstring{ L"circle" }, style.Words[L"cap-shape"]);
    VERIFY_IS_FALSE(te::SetStyleProperty(style, L"box-shadow", L"3px", error));
    VERIFY_IS_FALSE(error.empty());

    TestScope const scope{};
    auto const resolved = te::ResolveStyle(style, scope);
    VerifyNear(21.0, resolved.CornerRadius(0, 42.0, 42.0), 0.0, L"50% of a 42 pixel key");

    VERIFY_IS_TRUE(te::SetStyleProperty(style, L"border-radius", L"14px", error));
    VerifyNear(13.0, te::ResolveStyle(style, scope).CornerRadius(0, 60.0, 26.0), 0.0001, L"corners kept from overlapping");

    // CSS's saturate() leaves a gray gray, and saturate(0) makes anything gray.
    auto const gray = te::ApplyFilter(Hex(0x808080), { 0.2, 1.0 });
    VerifyColor(Hex(0x808080), gray, 0.003, L"a gray through saturate()");
    auto const red = te::ApplyFilter(Hex(0xFF0000), { 0.0, 1.0 });
    VerifyNear(red.R, red.G, 0.0001, L"no saturation, red and green");
}

void ThemeEngineTests::AThemeFileIsCheckedBeforeItIsUsed()
{
    auto const good = te::LoadTheme(MinimalTheme(L"\"tokens\": { \"surface\": \"#E4E3DF\", \"text\": \"#2A2A2D\" }"));
    VERIFY_IS_TRUE(good.Problems.empty());
    VERIFY_IS_NOT_NULL(good.Theme.get());

    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(L"not json"), L"isn't JSON"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(L"{ \"id\": \"x\", \"name\": \"X\", \"light\": {}, \"dark\": {} }"), L"themeEngine"));

    auto const newer = te::LoadTheme(L"{ \"themeEngine\": 2, \"id\": \"x\", \"name\": \"X\", \"light\": {}, \"dark\": {} }");
    VERIFY_IS_TRUE(newer.IsFromNewerVersion);
    VERIFY_IS_NULL(newer.Theme.get());

    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(L"{ \"themeEngine\": 1, \"id\": \"Bad Id\", \"name\": \"X\", \"light\": {}, \"dark\": {} }"), L"An id is"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"parts\": { \"cell\": { \"background\": \"surfac\" } }")), L"\"surfac\" isn't one of the theme's colors"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"tokens\": { \"surface\": \"oklch(from tint 0.9 c h)\" }")), L"\"tint\" can only be used in \"tinted\""));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"tokens\": { \"a\": \"track\" }, \"resources\": { \"SomeBrush\": \"a\" }")), L"depends on the track"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"resources\": { \"SomeBrush\": \"track\" }")), L"\"track\" can't be used here"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"tokens\": { \"a\": \"b\", \"b\": \"a\" }")), L"depends on itself"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"tinted\": { \"surface\": \"tint\" }")), L"needs one of the same name"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"colours\": {}")), L"isn't a section"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"parts\": { \"b\": { \"extends\": \"a\" } }")), L"isn't a part of this theme"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"parts\": { \"a\": { \"extends\": \"b\" }, \"b\": { \"extends\": \"a\" } }")), L"in a circle"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"parts\": { \"a\": { \"box-shadow\": \"raised\" } }")), L"isn't one of the theme's shadows"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"tokens\": { \"tint\": \"red\" }")), L"This name is taken"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"parts\": { \"a\": { \"opacity\": \"x\" } }")), L"parts.a.opacity"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"numbers\": { \"glaze\": \"1\" }")), L"This is a number"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"", L"\"extends\": \"dark\"")), L"can only start from"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"\"extends\": \"dark\"", L"\"extends\": \"light\"")), L"can't both start from each other"));
    VERIFY_IS_TRUE(HasProblemContaining(te::LoadTheme(MinimalTheme(L"", L"\"extends\": \"light\"", L"\"backdrops\": [ \"glass\" ], ")), L"solid, mica or acrylic"));

    // Nothing in a file makes the loader throw.
    for (auto const text : { L"", L"[]", L"{", L"{ \"themeEngine\": \"one\" }", L"{ \"themeEngine\": 1, \"light\": 5, \"dark\": [] }" })
    {
        auto const result = te::LoadTheme(text);
        VERIFY_IS_NULL(result.Theme.get());
        VERIFY_IS_FALSE(result.Problems.empty());
    }
}

void ThemeEngineTests::TheDarkLookStartsFromTheLightOne()
{
    auto const result = te::LoadTheme(MinimalTheme(
        L"\"tokens\": { \"surface\": \"#E4E3DF\", \"text\": \"#2A2A2D\" }, \"tinted\": { \"surface\": \"oklch(from tint 0.9 0.01 h)\" },"
        L"\"parts\": { \"cell\": { \"background\": \"surface\", \"color\": \"text\", \"border-radius\": \"10px\" } }",
        L"\"extends\": \"light\", \"tokens\": { \"surface\": \"#302F2D\" }, \"parts\": { \"cell\": { \"border-radius\": \"8px\" } }"));

    VERIFY_IS_TRUE(result.Problems.empty());

    auto const dark = te::ResolvedTheme::Resolve(result.Theme, true, std::nullopt);
    VerifyColor(Hex(0x302F2D), dark->Token(L"surface"), 0.00001, L"dark's own surface");
    VerifyColor(Hex(0x2A2A2D), dark->Token(L"text"), 0.00001, L"light's text, kept in dark");

    auto const cell = dark->Part(L"cell");
    VerifyNear(8.0, cell->Radius[0].Value, 0.0, L"dark's own radius");
    VerifyColor(Hex(0x2A2A2D), cell->Foreground.value_or(te::Color{}), 0.00001, L"light's text color for the cell");
    VerifyColor(Hex(0x302F2D), cell->Background.at(0).Solid, 0.00001, L"the cell takes dark's surface");

    // Light's tinted colors don't come with it: a window color leaves dark's surface alone.
    auto const tintedDark = te::ResolvedTheme::Resolve(result.Theme, true, Hex(0x031E24));
    VerifyColor(Hex(0x302F2D), tintedDark->Token(L"surface"), 0.00001, L"dark doesn't take light's tinted surface");
}

void ThemeEngineTests::APartInAStateTakesWhatTheStateChanges()
{
    auto const result = te::LoadTheme(MinimalTheme(
        L"\"tokens\": { \"face\": \"#ECEBE7\", \"pressed\": \"#DEDDD8\", \"shade\": \"#504C44\" },"
        L"\"shadows\": { \"raised\": \"3px 4px 11px oklch(from shade l c h / .18)\", \"lifted\": \"0 0 0 2px #2B9BDB, raised\" },"
        L"\"parts\": {"
        L"  \"cell\": { \"background\": \"face\", \"box-shadow\": \"raised\", \"note-color\": \"track\","
        L"            \"states\": { \"playing\": { \"background\": \"pressed\", \"box-shadow\": \"none\" }, \"muted\": { \"opacity\": 0.7 }, \"selected\": { \"box-shadow\": \"lifted\" } } },"
        L"  \"pad\": { \"extends\": \"cell\", \"border-radius\": \"50%\" },"
        L"  \"wash\": { \"background\": \"color-mix(in srgb, track 26%, white)\" } }"));

    VERIFY_IS_TRUE(result.Problems.empty());
    auto const theme = te::ResolvedTheme::Resolve(result.Theme, false, std::nullopt);

    auto const resting = theme->Part(L"cell");
    VerifyColor(Hex(0xECEBE7), resting->Background.at(0).Solid, 0.00001, L"the resting face");
    VERIFY_ARE_EQUAL(1u, resting->Shadow.size());
    VerifyNear(0.18, resting->Shadow[0].Value.A, 0.0001, L"the named shadow's color");

    auto const playingMuted = theme->Part(L"cell", { L"playing", L"muted" });
    VerifyColor(Hex(0xDEDDD8), playingMuted->Background.at(0).Solid, 0.00001, L"playing presses it");
    VERIFY_IS_TRUE(playingMuted->Shadow.empty());
    VerifyNear(0.7, playingMuted->Opacity, 0.0, L"muted fades it");

    // A shadow can name another, as CSS does with var().
    auto const selected = theme->Part(L"cell", { L"selected" });
    VERIFY_ARE_EQUAL(2u, selected->Shadow.size());
    VerifyNear(2.0, selected->Shadow[0].Spread, 0.0, L"the ring comes first");

    auto const pad = theme->Part(L"pad", { L"playing" });
    VerifyColor(Hex(0xDEDDD8), pad->Background.at(0).Solid, 0.00001, L"a part that extends another takes its states");
    VERIFY_IS_TRUE(pad->Radius[0].Percent);

    auto const wash = theme->Part(L"wash", {}, Hex(0xFF8A65));
    VerifyColor({ 1.0, 0.880706, 0.84298, 1.0 }, wash->Background.at(0).Solid, 0.00001, L"a part worked out for a track");
    VerifyColor(Hex(0xFF8A65), theme->Part(L"cell", {}, Hex(0xFF8A65))->ColorOr(L"note-color", {}), 0.00001, L"a value of the part's own");

    VERIFY_IS_FALSE(theme->HasPart(L"no-such-part"));
    VERIFY_IS_FALSE(theme->Part(L"no-such-part")->HasPlate());
}

void ThemeEngineTests::TheWindowColorOnlyChangesTintedColors()
{
    auto const material = te::LoadTheme(MinimalTheme(
        L"\"tokens\": { \"surface\": \"#E4E3DF\", \"text\": \"#2A2A2D\" }, \"numbers\": { \"glaze\": 1 },"
        L"\"tinted\": { \"surface\": \"oklch(from tint 0.915 calc(min(c * 0.45, 0.012) * glaze) h)\" }",
        L"\"extends\": \"light\"",
        L"\"windowColor\": \"material\", "));

    VERIFY_IS_TRUE(material.Problems.empty());

    auto const plain = te::ResolvedTheme::Resolve(material.Theme, false, std::nullopt);
    VerifyColor(Hex(0xE4E3DF), plain->Token(L"surface"), 0.00001, L"no window color, no tint");

    auto const tinted = te::ResolvedTheme::Resolve(material.Theme, false, Hex(0x031E24));
    auto const surface = te::ToOklch(tinted->Token(L"surface"));
    VerifyNear(0.915, surface.L, 0.0001, L"a tinted surface keeps the theme's lightness");
    VerifyNear(0.012, surface.C, 0.0001, L"and takes the tint's color, capped");
    VerifyNear(214.855, surface.H, 0.1, L"in the tint's hue");
    VerifyColor(Hex(0x2A2A2D), tinted->Token(L"text"), 0.00001, L"a color with no tinted version stays");

    auto const backdrop = te::LoadTheme(MinimalTheme(
        L"\"tokens\": { \"surface\": \"#E4E3DF\" }, \"tinted\": { \"surface\": \"tint\" }",
        L"\"extends\": \"light\"",
        L"\"windowColor\": \"backdrop\", "));

    auto const backdropTheme = te::ResolvedTheme::Resolve(backdrop.Theme, false, Hex(0x031E24));
    VERIFY_IS_FALSE(backdropTheme->Tint().has_value());
    VerifyColor(Hex(0xE4E3DF), backdropTheme->Token(L"surface"), 0.00001, L"a backdrop theme leaves its colors alone");
}

void ThemeEngineTests::EveryBuiltInThemeLoadsAndResolves()
{
    std::vector<std::wstring> ids{};

    for (auto const& entry : std::filesystem::directory_iterator{ ThemesFolder() })
    {
        if (entry.path().extension() != L".json")
        {
            continue;
        }

        auto const name = entry.path().stem().wstring();
        Log::Comment(String().Format(L"Theme %s", name.c_str()));

        auto const theme = LoadBuiltIn(name);

        if (theme == nullptr)
        {
            continue;
        }

        VERIFY_ARE_EQUAL(name, theme->Id);
        ids.push_back(theme->Id);
        VERIFY_IS_FALSE(theme->Name.empty());
        VERIFY_IS_FALSE(theme->Description.empty());

        for (auto const dark : { false, true })
        {
            for (auto const tint : { std::optional<te::Color>{}, std::optional<te::Color>{ Hex(0x031E24) }, std::optional<te::Color>{ Hex(0x33192F) },
                std::optional<te::Color>{ Hex(0x808080) }, std::optional<te::Color>{ Hex(0xFF0000) }, std::optional<te::Color>{ Hex(0xFFFF00) } })
            {
                auto const resolved = te::ResolvedTheme::Resolve(theme, dark, tint);
                auto const& mode = theme->Mode(dark);

                for (auto const& [token, expression] : mode.Tokens)
                {
                    auto const color = resolved->Token(token, Hex(0x6CCB5F), {});
                    VERIFY_IS_TRUE(std::isfinite(color.R) && std::isfinite(color.G) && std::isfinite(color.B) && std::isfinite(color.A));
                }

                for (auto const& [part, spec] : mode.Parts)
                {
                    resolved->Part(part, {}, Hex(0xFF8A65));

                    for (auto const& [state, style] : spec.States)
                    {
                        std::wstring_view const states[]{ state };
                        resolved->Part(part, std::span<std::wstring_view const>{ states }, Hex(0x5BC0EB));
                    }
                }

                for (auto const& resource : resolved->Resources())
                {
                    VERIFY_IS_FALSE(resource.Key.empty());
                }
            }
        }
    }

    VERIFY_IS_TRUE(std::find(ids.begin(), ids.end(), L"night-drive") != ids.end());
}

void ThemeEngineTests::NightDriveDrawsWhatTheBuiltInPaletteDid()
{
    using midisequencer::Palette;

    struct Field
    {
        midisequencer::Color Palette::* Member;
        wchar_t const* Token;
    };

    static constexpr Field fields[]{
        { &Palette::Text, L"text" }, { &Palette::Text2, L"text2" }, { &Palette::Text3, L"text3" }, { &Palette::Divider, L"divider" },
        { &Palette::StrokeStrong, L"strokeStrong" }, { &Palette::Accent, L"accent" }, { &Palette::Play, L"play" }, { &Palette::Record, L"record" },
        { &Palette::RecordText, L"recordText" }, { &Palette::Mute, L"mute" }, { &Palette::Solo, L"solo" }, { &Palette::Caution, L"caution" },
        { &Palette::LaneBackground, L"laneBackground" }, { &Palette::FolderLaneBackground, L"folderLaneBackground" },
        { &Palette::LauncherBackground, L"launcherBackground" }, { &Palette::LauncherEdge, L"launcherEdge" }, { &Palette::BarLine, L"barLine" },
        { &Palette::BeatLine, L"beatLine" }, { &Palette::RulerBackground, L"rulerBackground" }, { &Palette::RulerBarLine, L"rulerBarLine" },
        { &Palette::RulerBeatLine, L"rulerBeatLine" }, { &Palette::LoopFill, L"loopFill" }, { &Palette::LoopStroke, L"loopStroke" },
        { &Palette::Playhead, L"playhead" }, { &Palette::EmptySlotFill, L"emptySlotFill" }, { &Palette::EmptySlotStroke, L"emptySlotStroke" },
        { &Palette::EmptySlotGlyph, L"emptySlotGlyph" }, { &Palette::ArmedSlotGlyph, L"armedSlotGlyph" }, { &Palette::SelectionStroke, L"selectionStroke" },
        { &Palette::OverlayStripeA, L"overlayStripeA" }, { &Palette::OverlayStripeB, L"overlayStripeB" }, { &Palette::RollBackground, L"rollBackground" },
        { &Palette::RollBlackKeyRow, L"rollBlackKeyRow" }, { &Palette::RollWhiteKeyRow, L"rollWhiteKeyRow" }, { &Palette::RollOctaveLine, L"rollOctaveLine" },
        { &Palette::RollBarLine, L"rollBarLine" }, { &Palette::RollBeatLine, L"rollBeatLine" }, { &Palette::RollStepLine, L"rollStepLine" },
        { &Palette::KeyWhite, L"keyWhite" }, { &Palette::KeyBlack, L"keyBlack" }, { &Palette::KeyEdge, L"keyEdge" }, { &Palette::KeyLabel, L"keyLabel" },
        { &Palette::NoteEdge, L"noteEdge" }, { &Palette::LaneEdge, L"laneEdge" }, { &Palette::TempoLine, L"tempoLine" },
        { &Palette::TempoLabelFill, L"tempoLabelFill" }, { &Palette::TempoLabelStroke, L"tempoLabelStroke" }, { &Palette::TempoLabelText, L"tempoLabelText" },
        { &Palette::HandleFill, L"handleFill" },
    };

    auto const theme = LoadBuiltIn(L"night-drive");
    VERIFY_IS_TRUE(theme->WindowColor == te::WindowColorUse::Backdrop);
    VERIFY_IS_TRUE(theme->AllowsBackdrop(L"mica"));
    VERIFY_IS_TRUE(theme->AllowsBackdrop(L"acrylic"));

    auto const sameColor = [](midisequencer::Color a, midisequencer::Color b, int32_t tolerance)
    {
        auto const close = [tolerance](uint8_t x, uint8_t y) { return std::abs(static_cast<int32_t>(x) - static_cast<int32_t>(y)) <= tolerance; };
        return close(a.A, b.A) && close(a.R, b.R) && close(a.G, b.G) && close(a.B, b.B);
    };

    for (auto const light : { true, false })
    {
        // The window color is the backdrop's, so it changes none of these.
        for (auto const tint : { std::optional<te::Color>{}, std::optional<te::Color>{ Hex(0x031E24) } })
        {
            auto const resolved = te::ResolvedTheme::Resolve(theme, !light, tint);
            auto const fromTheme = midisequencer::MakePalette(resolved);
            auto const builtIn = midisequencer::MakePalette(light);

            for (auto const& field : fields)
            {
                VERIFY_IS_TRUE(resolved->HasToken(field.Token));

                auto const expected = builtIn.*field.Member;
                auto const actual = fromTheme.*field.Member;

                if (!sameColor(expected, actual, 0))
                {
                    Log::Comment(String().Format(L"%s %s: expected #%02X%02X%02X%02X, got #%02X%02X%02X%02X", light ? L"light" : L"dark", field.Token,
                        expected.A, expected.R, expected.G, expected.B, actual.A, actual.R, actual.G, actual.B));
                }

                VERIFY_IS_TRUE(sameColor(expected, actual, 0));
            }

            for (auto const rgb : { 0xFF8A65u, 0x5BC0EBu, 0xB4A7FFu, 0x6CCB5Fu, 0xF7C948u, 0xBDBDBDu, 0x000000u, 0xFFFFFFu })
            {
                auto const track = midisequencer::FromRgb(rgb);

                VERIFY_IS_TRUE(sameColor(midisequencer::ClipFill(builtIn, track), midisequencer::ClipFill(fromTheme, track), 1));
                VERIFY_IS_TRUE(sameColor(midisequencer::ClipStroke(builtIn, track), midisequencer::ClipStroke(fromTheme, track), 1));
                VERIFY_IS_TRUE(sameColor(midisequencer::ClipCaption(builtIn, track), midisequencer::ClipCaption(fromTheme, track), 1));
                VERIFY_IS_TRUE(sameColor(midisequencer::CellFill(builtIn, track, false), midisequencer::CellFill(fromTheme, track, false), 1));
                VERIFY_IS_TRUE(sameColor(midisequencer::CellFill(builtIn, track, true), midisequencer::CellFill(fromTheme, track, true), 1));
                VERIFY_IS_TRUE(sameColor(midisequencer::CellStroke(builtIn, track), midisequencer::CellStroke(fromTheme, track), 1));
                VERIFY_IS_TRUE(sameColor(midisequencer::SummaryFill(builtIn, track), midisequencer::SummaryFill(fromTheme, track), 1));
                VERIFY_IS_TRUE(sameColor(midisequencer::SummaryStroke(builtIn, track), midisequencer::SummaryStroke(fromTheme, track), 1));
            }
        }
    }
}

namespace
{
    struct ReadablePair
    {
        wchar_t const* Ink;
        wchar_t const* Paper;
        double Least;
    };

    // Each pair of tokens, in light and dark, with no window color and five very different ones.
    void VerifyReadable(std::wstring_view name, std::initializer_list<ReadablePair> pairs)
    {
        auto const theme = LoadBuiltIn(name);
        VERIFY_IS_NOT_NULL(theme.get());

        for (auto const dark : { false, true })
        {
            for (auto const tint : { std::optional<te::Color>{}, std::optional<te::Color>{ Hex(0x031E24) }, std::optional<te::Color>{ Hex(0x33192F) },
                std::optional<te::Color>{ Hex(0xFF0000) }, std::optional<te::Color>{ Hex(0xFFFF00) }, std::optional<te::Color>{ Hex(0x0000FF) } })
            {
                auto const resolved = te::ResolvedTheme::Resolve(theme, dark, tint);

                for (auto const& pair : pairs)
                {
                    auto const paper = resolved->Token(pair.Paper);
                    auto const ink = te::Over(resolved->Token(pair.Ink), paper);
                    auto const ratio = te::ContrastRatio(ink, paper);

                    if (ratio < pair.Least)
                    {
                        Log::Error(String().Format(L"%s on %s, %s, tint %s: %.2f:1, under %.1f:1", pair.Ink, pair.Paper, dark ? L"dark" : L"light",
                            tint.has_value() ? L"set" : L"none", ratio, pair.Least));
                    }
                }
            }
        }
    }
}

void ThemeEngineTests::AluminumTextIsReadableInEveryColor()
{
    // The names on the plates and the timeline, the readouts, and the words in the accent.
    VerifyReadable(L"aluminum", {
        { L"text", L"plate", 7.0 }, { L"text", L"plateLight", 7.0 }, { L"text", L"hdrSel", 7.0 }, { L"text2", L"plate", 4.5 },
        { L"text3", L"plate", 3.0 }, { L"text", L"clipBottom", 7.0 }, { L"accentInk", L"plate", 4.5 }, { L"lcdInk", L"lcdGlass", 7.0 },
        { L"padInk", L"graphiteLo", 7.0 },
    });
}

void ThemeEngineTests::VellumTextIsReadableInEveryColor()
{
    // Ink on the paper, the editor's sheet and the washes, and the vermilion words.
    VerifyReadable(L"vellum", {
        { L"text", L"paper", 7.0 }, { L"text", L"sheet", 7.0 }, { L"text", L"wash", 7.0 }, { L"text2", L"paper", 4.5 },
        { L"text3", L"paper", 4.5 }, { L"accentInk", L"paper", 4.5 }, { L"recordText", L"paper", 4.5 }, { L"indigo", L"paper", 4.5 },
        { L"play", L"paper", 3.0 }, { L"sheet", L"ink", 7.0 }, { L"sheet", L"vermilion", 3.0 }, { L"vermilion", L"paper", 3.0 },
    });
}
void ThemeEngineTests::CanvasSizesAreWholePixels()
{
    // The fewest DIPs that are whole pixels at each scale Windows offers.
    VERIFY_ARE_EQUAL(1.0, te::WholePixelStep(1.0));
    VERIFY_ARE_EQUAL(4.0, te::WholePixelStep(1.25));
    VERIFY_ARE_EQUAL(2.0, te::WholePixelStep(1.5));
    VERIFY_ARE_EQUAL(4.0, te::WholePixelStep(1.75));
    VERIFY_ARE_EQUAL(1.0, te::WholePixelStep(2.0));
    VERIFY_ARE_EQUAL(4.0, te::WholePixelStep(2.25));
    VERIFY_ARE_EQUAL(2.0, te::WholePixelStep(2.5));
    VERIFY_ARE_EQUAL(1.0, te::WholePixelStep(0.0));

    // Win2D#989's canvas: 1311 by 1243 pixels at 175%, which layout gives as 749.14 by 710.29 DIPs.
    VERIFY_ARE_EQUAL(748.0, te::WholePixelLength(1311.0 / 1.75, 1.75));
    VERIFY_ARE_EQUAL(708.0, te::WholePixelLength(1243.0 / 1.75, 1.75));

    // A size already whole stays, even written a hair short.
    VERIFY_ARE_EQUAL(588.0, te::WholePixelLength(588.0, 1.75));
    VERIFY_ARE_EQUAL(588.0, te::WholePixelLength(1029.0 / 1.75 - 1e-9, 1.75));
    VERIFY_ARE_EQUAL(301.0, te::WholePixelLength(301.6, 1.0));
    VERIFY_ARE_EQUAL(0.0, te::WholePixelLength(-3.0, 1.75));

    // Every length that comes out is a whole number of pixels, and no longer than what's there.
    for (auto const scale : { 1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 3.0 })
    {
        for (int pixels = 0; pixels < 3000; pixels += 7)
        {
            auto const available = static_cast<double>(pixels) / scale;
            auto const length = te::WholePixelLength(available, scale);
            auto const out = length * scale;

            VERIFY_IS_TRUE(length <= available + 1e-6);
            VERIFY_IS_TRUE(std::abs(out - std::round(out)) < 1e-9);
            VERIFY_IS_TRUE(std::abs(length - std::round(length)) < 1e-9);
        }
    }
}