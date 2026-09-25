// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace glass
{
    constexpr int32_t ThemeHueSlotCount = 6;

    // Straight 8 bits per channel. The surface never needs more, and a theme file a person can
    // read and hand-edit is worth more here than a wide gamut nobody asked for.
    struct ThemeColor
    {
        uint8_t R{ 0 };
        uint8_t G{ 0 };
        uint8_t B{ 0 };
        uint8_t A{ 255 };

        bool operator==(ThemeColor const& other) const noexcept
        {
            return R == other.R && G == other.G && B == other.B && A == other.A;
        }
    };

    enum class DeckKind
    {
        SolidColor = 0,
        Gradient = 1,
        Image = 2,
    };

    enum class LabelPlacement
    {
        Inside = 0,
        Below = 1,
        None = 2,
    };

    // Where the rim color comes from. Bigwig needs a neutral edge so that orange only ever means
    // "this is the value", which is what keeps a dense page readable.
    enum class RimSource
    {
        ControlHue = 0,
        NeutralEdge = 1,
        None = 2,
    };

    enum class ValueStripPlacement
    {
        Bottom = 0,
        Top = 1,
        None = 2,
    };

    enum class ValueIndicatorStyle
    {
        SolidArc = 0,
        SegmentedLamps = 1,
    };

    // The cap on a fader. Studio Dark's is a neutral machined bar with a hairline of the
    // control's hue through it; the tonal themes and Bigwig make the whole cap the hue.
    enum class ThumbStyle
    {
        None = 0,
        Neutral = 1,
        Hue = 2,
    };

    struct ThemeDeck
    {
        DeckKind Kind{ DeckKind::SolidColor };
        ThemeColor Color{};
        ThemeColor GradientEndColor{};

        // A bare file name inside the shared assets folder, never a path out of it.
        std::wstring ImageFileName{};
    };

    // A theme is six hue slots, a deck, and a handful of control defaults. A control stores a
    // slot rather than a color, so switching theme is a six color operation instead of a
    // redesign.
    struct Theme
    {
        std::wstring Name{};

        // Set on the ones that ship, so the UI can offer "reset" and refuse to overwrite them.
        bool IsBuiltIn{ false };

        std::array<ThemeColor, ThemeHueSlotCount> HueSlots{};
        ThemeDeck Deck{};

        int32_t CornerRadius{ 7 };

        // 0 is opaque, 100 is the original glass. The one skeuomorphic thing in the app, and it
        // is doing real work: it lets a themed deck show through so a surface reads as one object
        // rather than a scatter of stickers.
        int32_t GlassTintPercent{ 86 };

        // What the glass is made of. Smoked slate, not black: a plate of black at 86 percent
        // over a near-black deck comes out at almost zero, and the control stops reading as a
        // piece of glass sitting on the surface and starts reading as a hole cut out of it.
        ThemeColor GlassColor{ 0x11, 0x14, 0x1A, 255 };

        int32_t GlowStrength{ 60 };

        // The sheen down the top of a plate, as a percentage of white. It is what makes a plate
        // read as a raised piece of glass rather than a filled rectangle. Per theme rather than
        // a constant, because a white sheen on a near-white plate is just haze.
        int32_t PlateSheenPercent{ 6 };

        // How hard a plate sits above the deck, 0 to 100. The tonal themes deliberately carry
        // elevation as a tint of the surface's own color instead, and set this to zero.
        int32_t PlateElevation{ 55 };

        // The far end of the value bar, as a fraction of the hue's own alpha. The bar is
        // brightest where the value is and falls away behind it, which is what stops a long
        // fader reading as a flat stripe. 1.0 is a solid bar.
        double PipeFalloff{ 0.35 };

        ThumbStyle Thumb{ ThumbStyle::Neutral };

        // The two ends of a neutral cap. Ignored when Thumb is not Neutral.
        ThemeColor ThumbColor{ 0x31, 0x39, 0x45, 255 };
        ThemeColor ThumbEndColor{ 0x16, 0x1A, 0x21, 255 };

        LabelPlacement Labels{ LabelPlacement::Below };

        // Pigment. A wash of the control's own hue at rest, instead of an outline. This is what
        // makes the tonal themes read as a different family rather than a recolour.
        double FillAtRest{ 0.0 };

        // Pigment. The empty part of a fader slot or a knob arc. The dark themes get away with
        // hardcoding this black, which is a gap in the model rather than a cost of those themes:
        // the first customer to build a light theme of their own would have hit it.
        ThemeColor TrackColor{ 0, 0, 0, 255 };

        // The control plate itself. Alpha 0 means work it out from the two properties above it:
        // black at the glass tint, washed with the control's own hue where the theme is tonal.
        // Every theme but one wants that. Bigwig does not - its plate is a neutral raised grey,
        // because its whole idea is that orange only ever means "this is the value".
        ThemeColor PlateColor{ 0, 0, 0, 0 };

        // Bigwig.
        RimSource Rim{ RimSource::ControlHue };
        ThemeColor NeutralRimColor{ 90, 90, 90, 255 };
        ValueStripPlacement ValueStrip{ ValueStripPlacement::Bottom };
        ValueIndicatorStyle ValueIndicator{ ValueIndicatorStyle::SolidArc };

        int32_t LampCount{ 24 };

        // Below this, the lamps stop separating and the ring reads as a fine comb. A knob that
        // small falls back to the solid arc on its own rather than making somebody notice.
        int32_t MinimumLampRingSize{ 48 };
    };

    // The nine that ship. Studio Dark first, because it is the default and the one that stays
    // readable on the densest page.
    std::vector<Theme> const& BuiltInThemes() noexcept;

    Theme const* FindBuiltInTheme(_In_ std::wstring const& name) noexcept;

    // ---- contrast, measured rather than guessed ----

    // Relative luminance, per WCAG. Exposed because the contrast figure is meaningless without it
    // and a test that only checks the ratio cannot tell which side is wrong.
    double RelativeLuminance(_In_ ThemeColor const& color) noexcept;

    // WCAG contrast ratio, 1.0 to 21.0.
    double ContrastRatio(_In_ ThemeColor const& first, _In_ ThemeColor const& second) noexcept;

    // The bar a hue slot has to clear against the deck to be legible on a stage. A control rim is
    // a thin line and a value pipe is a thin bar, so this is the large-text threshold rather than
    // the body-text one; below it, a slot is called out with what to do about it.
    constexpr double MinimumSlotContrast = 3.0;

    struct SlotContrast
    {
        int32_t SlotIndex{ 0 };
        double Ratio{ 0.0 };
        bool MeetsMinimum{ false };
    };

    // Every slot measured against the deck. An image deck cannot be measured, so it reports
    // against the deck color, which is what the image is laid over.
    std::vector<SlotContrast> MeasureContrast(_In_ Theme const& theme) noexcept;
}
