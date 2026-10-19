// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Draws a part's look with Win2D the way a browser draws the same CSS: outer box shadows, the
// background layers, inset box shadows, the border and the outline, faded by its opacity and with
// its filter applied to its colors.
//
// Shadows are blurred once per shape, color and display scale and kept as nine-slice images, so a
// timeline with hundreds of clips costs a few image draws per clip rather than a blur each.
//
// Like ThemeXaml, this includes the app's pch.h, which has to bring in Win2D.

#include "ThemeResolver.h"

namespace themeengine
{
    struct PaintRect
    {
        float X{ 0 };
        float Y{ 0 };
        float Width{ 0 };
        float Height{ 0 };
    };

    // The rectangle a part is drawn in inside the space it's given: the space less its margin, or,
    // for a part with its own width and height, that size at the space's middle.
    PaintRect PartRect(_In_ ResolvedStyle const& style, _In_ PaintRect const& space) noexcept;

    // The part's outline as a path, with each corner's own radius.
    winrt::Microsoft::Graphics::Canvas::Geometry::CanvasGeometry PartShape(
        _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreator const& creator,
        _In_ ResolvedStyle const& style,
        _In_ PaintRect const& rect,
        _In_ float inflate = 0.0f);

    class ThemePainter
    {
    public:
        // Draws the part's shadows, background, border and outline in the rectangle. The device
        // and display scale come from the canvas being drawn. A caller that draws what's inside
        // the part as well fades the whole thing itself, and passes withOpacity false.
        void Paint(
            _In_ winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ ResolvedStyle const& style,
            _In_ PaintRect const& rect,
            _In_ bool withOpacity = true);

        // A brush for one background layer of the part, over the rectangle.
        winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush BackgroundBrush(
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreator const& creator,
            _In_ ResolvedBackground const& layer,
            _In_ FilterValue const& filter,
            _In_ PaintRect const& rect);

        // Drops every image it keeps: after the device is lost, or to save memory.
        void Reset() noexcept;

    private:
        struct NineSlice
        {
            winrt::Microsoft::Graphics::Canvas::CanvasRenderTarget Image{ nullptr };

            // the shape's corner zone, and how far the image reaches past the shape, in DIPs
            float Corner{ 0 };
            float Reach{ 0 };

            // Across or down, the image is the shape's own size rather than slices to stretch: a
            // shape shorter than its two corners, such as a long thin bar, is sliced only along
            // its length.
            bool WholeX{ false };
            bool WholeY{ false };
        };

        NineSlice const* OuterShadow(
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ ResolvedStyle const& style,
            _In_ ResolvedShadow const& shadow,
            _In_ PaintRect const& rect);

        NineSlice const* InsetShadow(
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ ResolvedStyle const& style,
            _In_ ResolvedShadow const& shadow,
            _In_ PaintRect const& rect);

        void DrawSlices(
            _In_ winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            _In_ NineSlice const& slices,
            _In_ PaintRect const& shape);

        std::unordered_map<std::wstring, NineSlice> m_shadows{};
        float m_dpi{ 0 };
        winrt::Microsoft::Graphics::Canvas::CanvasDevice m_device{ nullptr };
    };

    // What CSS's filter does to every color a part draws inside itself, as well as the part.
    winrt::Windows::UI::Color FilteredColor(_In_ Color const& color, _In_ FilterValue const& filter) noexcept;
}
