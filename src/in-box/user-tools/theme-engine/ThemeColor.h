// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Color math for the theme engine, done the way CSS does it, so a theme written from a CSS comp
// draws the colors the comp shows: sRGB with its transfer curve, OKLab and OKLCH, color-mix(), and
// the contrast ratio WCAG uses.
//
// Plain C++ with no Windows headers, so the unit tests and any app can compile it as it ships.

#include <sal.h>

#include <cstdint>
#include <optional>

namespace themeengine
{
    // An sRGB color with straight (not premultiplied) alpha, each channel 0 to 1. While a color is
    // being worked out its channels can leave that range; Clamped() is the color that gets drawn,
    // which is what a browser does with a color outside sRGB.
    struct Color
    {
        double R{ 0 };
        double G{ 0 };
        double B{ 0 };
        double A{ 1 };

        Color Clamped() const noexcept;

        // 0xAARRGGBB, each channel rounded from the clamped color.
        uint32_t ToArgb() const noexcept;

        static Color FromArgb(_In_ uint32_t argb) noexcept;
        static Color FromRgb8(_In_ uint8_t r, _In_ uint8_t g, _In_ uint8_t b, _In_ double alpha = 1.0) noexcept;

        friend bool operator==(Color const&, Color const&) = default;
    };

    struct Oklab
    {
        double L{ 0 };
        double A{ 0 };
        double B{ 0 };
    };

    // H is in degrees, 0 to 360.
    struct Oklch
    {
        double L{ 0 };
        double C{ 0 };
        double H{ 0 };
    };

    double SrgbToLinear(_In_ double channel) noexcept;
    double LinearToSrgb(_In_ double channel) noexcept;

    Oklab ToOklab(_In_ Color const& color) noexcept;
    Color FromOklab(_In_ Oklab const& lab, _In_ double alpha) noexcept;

    Oklch ToOklch(_In_ Color const& color) noexcept;
    Color FromOklch(_In_ Oklch const& lch, _In_ double alpha) noexcept;

    // Whether the color is inside sRGB, so it draws as written rather than clipped.
    bool IsInSrgbGamut(_In_ Color const& color, _In_ double tolerance = 0.0005) noexcept;

    // The most chroma a color can have at this lightness and hue and still be inside sRGB.
    double MaximumSrgbChroma(_In_ double lightness, _In_ double hue) noexcept;

    enum class MixSpace : int32_t
    {
        Srgb,
        SrgbLinear,
        Oklab,
        Oklch,
    };

    // CSS color-mix(in <space>, a pa%, b pb%): either share can be left out, interpolation is
    // premultiplied, shares that add up to less than 100% make the result that much see-through,
    // and in OKLCH the hue goes the shorter way round. Shares are 0 to 1, not percentages.
    Color MixColors(
        _In_ MixSpace space,
        _In_ Color const& a,
        _In_ std::optional<double> shareA,
        _In_ Color const& b,
        _In_ std::optional<double> shareB) noexcept;

    // The top color laid over the bottom one (source over).
    Color Over(_In_ Color const& top, _In_ Color const& bottom) noexcept;

    // WCAG relative luminance and contrast ratio, of the colors as drawn (clamped, alpha ignored).
    double RelativeLuminance(_In_ Color const& color) noexcept;
    double ContrastRatio(_In_ Color const& a, _In_ Color const& b) noexcept;
}
