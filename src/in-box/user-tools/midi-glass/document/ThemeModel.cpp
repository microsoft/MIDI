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

        Theme MakeDarkTheme(
            _In_ std::wstring name,
            _In_ uint32_t deck,
            _In_ std::array<uint32_t, ThemeHueSlotCount> const& hues) noexcept
        {
            Theme theme{};

            theme.Name = std::move(name);
            theme.IsBuiltIn = true;
            theme.Deck.Kind = DeckKind::SolidColor;
            theme.Deck.Color = Rgb(deck);

            for (size_t i = 0; i < hues.size(); ++i)
            {
                theme.HueSlots[i] = Rgb(hues[i]);
            }

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

                    studio.TrackColor = Rgb(0x1E2026);

                    list.push_back(studio);
                }

                {
                    auto neon = MakeDarkTheme(L"Neon Booth", 0x08040F,
                        { 0x00E5FF, 0x76FF03, 0xFFEA00, 0xFF1744, 0xD500F9, 0x1DE9B6 });

                    neon.TrackColor = Rgb(0x1E1430);

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

                    list.push_back(daylight);
                }

                {
                    auto amber = MakeDarkTheme(L"Amber Console", 0x120D06,
                        { 0xFFB300, 0xFFCC80, 0xFF8F00, 0xFFE082, 0xE65100, 0xFFF3E0 });

                    amber.TrackColor = Rgb(0x2C2010);

                    list.push_back(amber);
                }

                {
                    auto blueprint = MakeDarkTheme(L"Blueprint", 0x0A1929,
                        { 0x90CAF9, 0x64B5F6, 0xE3F2FD, 0x42A5F5, 0xB3E5FC, 0x1E88E5 });

                    blueprint.TrackColor = Rgb(0x16304D);

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
                    pigment.TrackColor = Rgb(0xE3DFD9);
                    pigment.CornerRadius = 14;

                    list.push_back(pigment);
                }

                {
                    auto pigment = MakeDarkTheme(L"Pigment Dark", 0x16151A,
                        { 0x64B5F6, 0x81C784, 0xFFB74D, 0xF06292, 0xBA68C8, 0x4DD0E1 });

                    pigment.GlassTintPercent = 0;
                    pigment.GlowStrength = 0;
                    pigment.FillAtRest = 0.18;
                    pigment.TrackColor = Rgb(0x2A2830);
                    pigment.CornerRadius = 14;

                    list.push_back(pigment);
                }

                // ---- Bigwig ----
                // The tonal machinery with the hue wash turned off, so the plate stays a neutral
                // grey and orange only ever means "this is the value" or "this is on".

                {
                    auto bigwig = MakeDarkTheme(L"Bigwig", 0x0A0A0A,
                        { 0xFF6A00, 0xFF6A00, 0xFF6A00, 0xFF6A00, 0xFF6A00, 0xFF6A00 });

                    bigwig.GlassTintPercent = 0;
                    bigwig.GlowStrength = 0;
                    bigwig.FillAtRest = 0.0;
                    bigwig.TrackColor = Rgb(0x2E2E2E);
                    bigwig.PlateColor = Rgb(0x3A3A3A);
                    bigwig.Rim = RimSource::NeutralEdge;
                    bigwig.NeutralRimColor = Rgb(0x5A5A5A);
                    bigwig.ValueStrip = ValueStripPlacement::Top;
                    bigwig.ValueIndicator = ValueIndicatorStyle::SegmentedLamps;
                    bigwig.LampCount = 24;
                    bigwig.CornerRadius = 4;

                    list.push_back(bigwig);
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
