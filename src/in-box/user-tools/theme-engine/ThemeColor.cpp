// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThemeColor.h"

#include <algorithm>
#include <cmath>

namespace themeengine
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;

        // Below this an OKLCH color has no hue to speak of, and CSS treats its hue as missing.
        constexpr double AchromaticChroma = 0.000001;

        double Clamp01(double value) noexcept
        {
            if (std::isnan(value))
            {
                return 0.0;
            }

            return std::clamp(value, 0.0, 1.0);
        }

        uint32_t Channel8(double value) noexcept
        {
            return static_cast<uint32_t>(std::lround(Clamp01(value) * 255.0));
        }

        double NormalizeHue(double hue) noexcept
        {
            if (!std::isfinite(hue))
            {
                return 0.0;
            }

            hue = std::fmod(hue, 360.0);
            return hue < 0.0 ? hue + 360.0 : hue;
        }

        // The three components of a color in an interpolation space, with the hue (if any) last.
        struct Components
        {
            double C0{ 0 };
            double C1{ 0 };
            double C2{ 0 };
            bool HueMissing{ false };
        };

        Components ToSpace(MixSpace space, Color const& color) noexcept
        {
            switch (space)
            {
            case MixSpace::SrgbLinear:
                return { SrgbToLinear(color.R), SrgbToLinear(color.G), SrgbToLinear(color.B), false };

            case MixSpace::Oklab:
            {
                auto const lab = ToOklab(color);
                return { lab.L, lab.A, lab.B, false };
            }

            case MixSpace::Oklch:
            {
                auto const lch = ToOklch(color);
                return { lch.L, lch.C, lch.H, lch.C < AchromaticChroma };
            }

            case MixSpace::Srgb:
            default:
                return { color.R, color.G, color.B, false };
            }
        }

        Color FromSpace(MixSpace space, Components const& c, double alpha) noexcept
        {
            switch (space)
            {
            case MixSpace::SrgbLinear:
                return { LinearToSrgb(c.C0), LinearToSrgb(c.C1), LinearToSrgb(c.C2), alpha };

            case MixSpace::Oklab:
                return FromOklab({ c.C0, c.C1, c.C2 }, alpha);

            case MixSpace::Oklch:
                return FromOklch({ c.C0, c.C1, c.C2 }, alpha);

            case MixSpace::Srgb:
            default:
                return { c.C0, c.C1, c.C2, alpha };
            }
        }
    }

    Color Color::Clamped() const noexcept
    {
        return { Clamp01(R), Clamp01(G), Clamp01(B), Clamp01(A) };
    }

    uint32_t Color::ToArgb() const noexcept
    {
        return (Channel8(A) << 24) | (Channel8(R) << 16) | (Channel8(G) << 8) | Channel8(B);
    }

    _Use_decl_annotations_
    Color Color::FromArgb(uint32_t argb) noexcept
    {
        return {
            static_cast<double>((argb >> 16) & 0xFF) / 255.0,
            static_cast<double>((argb >> 8) & 0xFF) / 255.0,
            static_cast<double>(argb & 0xFF) / 255.0,
            static_cast<double>((argb >> 24) & 0xFF) / 255.0 };
    }

    _Use_decl_annotations_
    Color Color::FromRgb8(uint8_t r, uint8_t g, uint8_t b, double alpha) noexcept
    {
        return { r / 255.0, g / 255.0, b / 255.0, alpha };
    }

    _Use_decl_annotations_
    double SrgbToLinear(double channel) noexcept
    {
        auto const magnitude = std::abs(channel);
        auto const linear = magnitude <= 0.04045 ? magnitude / 12.92 : std::pow((magnitude + 0.055) / 1.055, 2.4);
        return std::copysign(linear, channel);
    }

    _Use_decl_annotations_
    double LinearToSrgb(double channel) noexcept
    {
        auto const magnitude = std::abs(channel);
        auto const encoded = magnitude <= 0.0031308 ? magnitude * 12.92 : 1.055 * std::pow(magnitude, 1.0 / 2.4) - 0.055;
        return std::copysign(encoded, channel);
    }

    _Use_decl_annotations_
    Oklab ToOklab(Color const& color) noexcept
    {
        auto const r = SrgbToLinear(color.R);
        auto const g = SrgbToLinear(color.G);
        auto const b = SrgbToLinear(color.B);

        auto const l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
        auto const m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
        auto const s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);

        return {
            0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s };
    }

    _Use_decl_annotations_
    Color FromOklab(Oklab const& lab, double alpha) noexcept
    {
        auto const l = lab.L + 0.3963377774 * lab.A + 0.2158037573 * lab.B;
        auto const m = lab.L - 0.1055613458 * lab.A - 0.0638541728 * lab.B;
        auto const s = lab.L - 0.0894841775 * lab.A - 1.2914855480 * lab.B;

        auto const l3 = l * l * l;
        auto const m3 = m * m * m;
        auto const s3 = s * s * s;

        return {
            LinearToSrgb(4.0767416621 * l3 - 3.3077115913 * m3 + 0.2309699292 * s3),
            LinearToSrgb(-1.2684380046 * l3 + 2.6097574011 * m3 - 0.3413193965 * s3),
            LinearToSrgb(-0.0041960863 * l3 - 0.7034186147 * m3 + 1.7076147010 * s3),
            alpha };
    }

    _Use_decl_annotations_
    Oklch ToOklch(Color const& color) noexcept
    {
        auto const lab = ToOklab(color);
        auto const chroma = std::sqrt(lab.A * lab.A + lab.B * lab.B);
        auto const hue = chroma < AchromaticChroma ? 0.0 : NormalizeHue(std::atan2(lab.B, lab.A) * 180.0 / Pi);

        return { lab.L, chroma, hue };
    }

    _Use_decl_annotations_
    Color FromOklch(Oklch const& lch, double alpha) noexcept
    {
        auto const chroma = std::max(0.0, lch.C);
        auto const radians = NormalizeHue(lch.H) * Pi / 180.0;

        return FromOklab({ lch.L, chroma * std::cos(radians), chroma * std::sin(radians) }, alpha);
    }

    _Use_decl_annotations_
    bool IsInSrgbGamut(Color const& color, double tolerance) noexcept
    {
        auto const inside = [tolerance](double channel) { return channel >= -tolerance && channel <= 1.0 + tolerance; };
        return inside(color.R) && inside(color.G) && inside(color.B);
    }

    _Use_decl_annotations_
    double MaximumSrgbChroma(double lightness, double hue) noexcept
    {
        if (lightness <= 0.0 || lightness >= 1.0)
        {
            return 0.0;
        }

        double low = 0.0;
        double high = 0.5;

        for (int step = 0; step < 40; ++step)
        {
            auto const middle = (low + high) / 2.0;

            if (IsInSrgbGamut(FromOklch({ lightness, middle, hue }, 1.0), 0.0))
            {
                low = middle;
            }
            else
            {
                high = middle;
            }
        }

        return low;
    }

    _Use_decl_annotations_
    Color MixColors(MixSpace space, Color const& a, std::optional<double> shareA, Color const& b, std::optional<double> shareB) noexcept
    {
        double pa = 0.5;
        double pb = 0.5;
        double alphaMultiplier = 1.0;

        if (shareA.has_value() && shareB.has_value())
        {
            auto const sum = *shareA + *shareB;

            if (sum <= 0.0)
            {
                return { 0, 0, 0, 0 };
            }

            if (sum < 1.0)
            {
                alphaMultiplier = sum;
            }

            pa = *shareA / sum;
            pb = *shareB / sum;
        }
        else if (shareA.has_value())
        {
            pa = std::clamp(*shareA, 0.0, 1.0);
            pb = 1.0 - pa;
        }
        else if (shareB.has_value())
        {
            pb = std::clamp(*shareB, 0.0, 1.0);
            pa = 1.0 - pb;
        }

        auto ca = ToSpace(space, a);
        auto cb = ToSpace(space, b);
        auto const alphaA = Clamp01(a.A);
        auto const alphaB = Clamp01(b.A);

        auto const polar = space == MixSpace::Oklch;

        if (polar)
        {
            // A missing hue takes the other color's, and the hue goes the shorter way round.
            if (ca.HueMissing && !cb.HueMissing)
            {
                ca.C2 = cb.C2;
            }
            else if (cb.HueMissing && !ca.HueMissing)
            {
                cb.C2 = ca.C2;
            }

            auto const difference = cb.C2 - ca.C2;

            if (difference > 180.0)
            {
                ca.C2 += 360.0;
            }
            else if (difference < -180.0)
            {
                cb.C2 += 360.0;
            }
        }

        // Premultiplied, except for a hue, which isn't a quantity of anything.
        auto const premultiply = [polar](Components c, double alpha)
        {
            c.C0 *= alpha;
            c.C1 *= alpha;

            if (!polar)
            {
                c.C2 *= alpha;
            }

            return c;
        };

        auto const pma = premultiply(ca, alphaA);
        auto const pmb = premultiply(cb, alphaB);

        auto const alpha = alphaA * pa + alphaB * pb;

        Components mixed{
            pma.C0 * pa + pmb.C0 * pb,
            pma.C1 * pa + pmb.C1 * pb,
            pma.C2 * pa + pmb.C2 * pb,
            false };

        if (alpha > 0.0)
        {
            mixed.C0 /= alpha;
            mixed.C1 /= alpha;

            if (!polar)
            {
                mixed.C2 /= alpha;
            }
        }

        if (polar)
        {
            mixed.C2 = NormalizeHue(mixed.C2);
        }

        return FromSpace(space, mixed, alpha * alphaMultiplier);
    }

    _Use_decl_annotations_
    Color Over(Color const& top, Color const& bottom) noexcept
    {
        auto const ta = Clamp01(top.A);
        auto const ba = Clamp01(bottom.A);
        auto const alpha = ta + ba * (1.0 - ta);

        if (alpha <= 0.0)
        {
            return { 0, 0, 0, 0 };
        }

        auto const blend = [&](double t, double b) { return (t * ta + b * ba * (1.0 - ta)) / alpha; };

        return { blend(top.R, bottom.R), blend(top.G, bottom.G), blend(top.B, bottom.B), alpha };
    }

    _Use_decl_annotations_
    double RelativeLuminance(Color const& color) noexcept
    {
        auto const c = color.Clamped();
        return 0.2126 * SrgbToLinear(c.R) + 0.7152 * SrgbToLinear(c.G) + 0.0722 * SrgbToLinear(c.B);
    }

    _Use_decl_annotations_
    double ContrastRatio(Color const& a, Color const& b) noexcept
    {
        auto const la = RelativeLuminance(a);
        auto const lb = RelativeLuminance(b);
        auto const lighter = std::max(la, lb);
        auto const darker = std::min(la, lb);

        return (lighter + 0.05) / (darker + 0.05);
    }
}
