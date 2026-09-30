// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "KeyColors.h"

namespace midikeyboard
{
    namespace
    {
        // WCAG AA for body text. Black or white always reaches it against a single color: the
        // better of the two can never fall below the square root of 21, about 4.58.
        constexpr double MinimumTextContrast = 4.5;

        // the lines between keys: plainly visible, while still a shade of the key color
        constexpr double MinimumLineContrast = 2.0;

        // how the built-in keys are shaded: lighter at the far end, darker toward the player
        constexpr double TopLighten = 0.18;
        constexpr double BottomDarken = 0.12;

        // how far the built-in keys' text leans toward the key color, softer than pure black or
        // white; custom keys lean the same way, less whenever that would hurt the contrast
        constexpr int32_t DarkTextTintPercent = 38;
        constexpr int32_t LightTextTintPercent = 25;

        struct Rgb
        {
            uint8_t R{ 0 };
            uint8_t G{ 0 };
            uint8_t B{ 0 };
        };

        constexpr Rgb Black{ 0, 0, 0 };
        constexpr Rgb White{ 255, 255, 255 };

        Rgb FromArgb(_In_ uint32_t argb) noexcept
        {
            return Rgb{
                static_cast<uint8_t>((argb >> 16) & 0xFF),
                static_cast<uint8_t>((argb >> 8) & 0xFF),
                static_cast<uint8_t>(argb & 0xFF) };
        }

        uint32_t ToArgb(_In_ Rgb const& color) noexcept
        {
            return 0xFF000000 |
                (static_cast<uint32_t>(color.R) << 16) |
                (static_cast<uint32_t>(color.G) << 8) |
                static_cast<uint32_t>(color.B);
        }

        uint8_t MixChannel(_In_ uint8_t from, _In_ uint8_t to, _In_ double amount) noexcept
        {
            return static_cast<uint8_t>(std::lround(from + (static_cast<double>(to) - from) * amount));
        }

        // Rounded here, so every contrast test below is made on the exact color that is drawn.
        Rgb Mix(_In_ Rgb const& from, _In_ Rgb const& to, _In_ double amount) noexcept
        {
            return Rgb{
                MixChannel(from.R, to.R, amount),
                MixChannel(from.G, to.G, amount),
                MixChannel(from.B, to.B, amount) };
        }

        double LinearChannel(_In_ uint8_t value) noexcept
        {
            auto const channel = value / 255.0;

            return channel <= 0.04045
                ? channel / 12.92
                : std::pow((channel + 0.055) / 1.055, 2.4);
        }

        // WCAG relative luminance
        double Luminance(_In_ Rgb const& color) noexcept
        {
            return 0.2126 * LinearChannel(color.R) +
                0.7152 * LinearChannel(color.G) +
                0.0722 * LinearChannel(color.B);
        }

        double Contrast(_In_ Rgb const& first, _In_ Rgb const& second) noexcept
        {
            auto const a = Luminance(first);
            auto const b = Luminance(second);

            return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
        }
    }

    _Use_decl_annotations_
    KeyPalette MakeKeyPalette(uint32_t keyColorArgb) noexcept
    {
        auto const base = FromArgb(keyColorArgb);

        Rgb top{ base };
        Rgb bottom{ base };
        bool darkText{ true };

        // Shading widens the range of colors under the text. On a mid tone that can leave
        // neither black nor white readable everywhere, so the shading backs off until one is.
        for (auto const shading : { 1.0, 0.75, 0.5, 0.25, 0.0 })
        {
            top = Mix(base, White, TopLighten * shading);
            bottom = Mix(base, Black, BottomDarken * shading);

            // dark text is hardest to read where the key is darkest, light text where it is lightest
            auto const darkTextContrast = Contrast(Black, bottom);
            auto const lightTextContrast = Contrast(White, top);

            darkText = darkTextContrast >= lightTextContrast;

            if (std::max(darkTextContrast, lightTextContrast) >= MinimumTextContrast)
            {
                break;
            }
        }

        auto const ink = darkText ? Black : White;
        auto const hardestBackground = darkText ? bottom : top;

        auto text = ink;

        for (auto percent = darkText ? DarkTextTintPercent : LightTextTintPercent; percent > 0; percent--)
        {
            auto const tinted = Mix(ink, base, percent / 100.0);

            if (Contrast(tinted, hardestBackground) >= MinimumTextContrast)
            {
                text = tinted;
                break;
            }
        }

        KeyPalette palette{ ToArgb(top), ToArgb(bottom), ToArgb(text) };
        palette.LineArgb = MakeKeyLineArgb(keyColorArgb, palette);

        return palette;
    }

    _Use_decl_annotations_
    uint32_t MakeKeyLineArgb(uint32_t keyColorArgb, KeyPalette const& palette) noexcept
    {
        auto const base = FromArgb(keyColorArgb);
        auto const top = FromArgb(palette.TopArgb);
        auto const bottom = FromArgb(palette.BottomArgb);

        // darker lines on a key with dark text, lighter ones on a key with light text
        auto const darkText = Luminance(FromArgb(palette.TextArgb)) < Luminance(bottom);
        auto const ink = darkText ? Black : White;
        auto const hardestBackground = darkText ? bottom : top;

        for (auto percent = 1; percent <= 100; percent++)
        {
            auto const shaded = Mix(base, ink, percent / 100.0);

            if (Contrast(shaded, hardestBackground) >= MinimumLineContrast)
            {
                return ToArgb(shaded);
            }
        }

        return ToArgb(ink);
    }
}
