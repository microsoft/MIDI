// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "DeckBrush.h"
#include "SurfaceColors.h"

#include <algorithm>

using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml::Hosting;

namespace glass
{
    namespace
    {
        winrt::Windows::UI::Color ToColor(_In_ ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        // A scan line is one screen pixel of the three, whatever the pitch is. Thicker and the
        // glass goes dark; thinner and it stops resolving on any display.
        constexpr double ScanLineThicknessPixels = 1.0;

        // A pitch small enough to be meaningless would still be asked to fill a page. This is
        // the ceiling on how many shapes one deck may cost.
        constexpr int32_t MaximumScanLines = 4000;

        // Where the corner fall-off starts and how far out it reaches, measured off the comps.
        constexpr float VignetteCenterY = 0.44f;
        constexpr float VignetteRadiusX = 1.12f;
        constexpr float VignetteRadiusY = 0.88f;
        constexpr float VignetteClearTo = 0.30f;
        constexpr float VignetteKnee = 0.70f;

        // The faceplate reflection runs across the upper left and is gone before the middle.
        // 142 degrees in the comp, which is this vector once the y axis is pointing down.
        constexpr float FaceplateRunX = 0.62f;
        constexpr float FaceplateRunY = 0.79f;
        constexpr float FaceplateKnee = 0.26f;
        constexpr float FaceplateEnd = 0.46f;

        // The grain is one tile, rendered offscreen once and then repeated. Drawing a speckle
        // per cell across a whole page would be a hundred thousand shapes; this is a couple of
        // thousand in the tile plus one cheap sprite per repeat.
        //
        // The tile is deliberately large. At 64 the eye finds the repeat; at 192 it does not.
        constexpr float GrainTileSize = 192.0f;
        constexpr float GrainCellSize = 3.0f;
        constexpr int32_t MaximumGrainTiles = 1200;

        // Roughly three cells in ten carry a speckle. Denser than that and it stops being
        // grain and starts being a second color.
        constexpr uint32_t GrainDensityOfTen = 3;

        // Deterministic, so a re-render is the same grain rather than a shimmer. A real random
        // source here would make every resize look like the panel was re-manufactured.
        uint32_t NextGrainNoise(_Inout_ uint32_t& state) noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            return state;
        }

        Visual BuildGrain(
            _In_ Compositor const& compositor,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height)
        {
            auto const strength = std::clamp(theme.Overlay.GrainPercent / 100.0, 0.0, 1.0);

            auto speckle = EffectiveGrainColor(theme);

            // Sandpaper is light AND dark, never one or the other. A single-sided speckle reads
            // as dirt on the panel instead of as texture in it.
            auto shade = ThemeColor{ 0, 0, 0, 0 };

            auto const lightAlpha = static_cast<uint8_t>(
                std::clamp(std::lround(255.0 * strength * 0.16), 0L, 255L));

            auto const darkAlpha = static_cast<uint8_t>(
                std::clamp(std::lround(255.0 * strength * 0.13), 0L, 255L));

            speckle.A = lightAlpha;
            shade.A = darkAlpha;

            auto const lightBrush = compositor.CreateColorBrush(ToColor(speckle));
            auto const darkBrush = compositor.CreateColorBrush(ToColor(shade));

            auto tile = compositor.CreateShapeVisual();
            tile.Size(float2{ GrainTileSize, GrainTileSize });

            auto const cells = static_cast<int32_t>(GrainTileSize / GrainCellSize);

            uint32_t state = 0x9E3779B9u;

            for (int32_t row = 0; row < cells; ++row)
            {
                for (int32_t column = 0; column < cells; ++column)
                {
                    auto const roll = NextGrainNoise(state);

                    if (roll % 10u >= GrainDensityOfTen)
                    {
                        continue;
                    }

                    auto geometry = compositor.CreateRectangleGeometry();
                    geometry.Size(float2{ GrainCellSize, GrainCellSize });
                    geometry.Offset(float2{ column * GrainCellSize, row * GrainCellSize });

                    auto shape = compositor.CreateSpriteShape(geometry);
                    shape.FillBrush((roll >> 8) % 2u == 0u ? lightBrush : darkBrush);

                    tile.Shapes().Append(shape);
                }
            }

            // Rendered once into an offscreen surface, then repeated. This is the same trick the
            // plate shadows use to get a mask the shape of a control.
            auto surface = compositor.CreateVisualSurface();
            surface.SourceVisual(tile);
            surface.SourceSize(float2{ GrainTileSize, GrainTileSize });

            auto const brush = compositor.CreateSurfaceBrush(surface);

            auto root = compositor.CreateContainerVisual();
            root.Size(float2{ width, height });
            root.Clip(compositor.CreateInsetClip());

            auto const across = static_cast<int32_t>(std::ceil(width / GrainTileSize));
            auto const down = static_cast<int32_t>(std::ceil(height / GrainTileSize));

            auto placed = 0;

            for (int32_t row = 0; row < down && placed < MaximumGrainTiles; ++row)
            {
                for (int32_t column = 0; column < across && placed < MaximumGrainTiles; ++column)
                {
                    auto sprite = compositor.CreateSpriteVisual();
                    sprite.Size(float2{ GrainTileSize, GrainTileSize });
                    sprite.Offset(float3{ column * GrainTileSize, row * GrainTileSize, 0.0f });
                    sprite.Brush(brush);

                    root.Children().InsertAtTop(sprite);
                    ++placed;
                }
            }

            return root;
        }

        Visual BuildScanLines(
            _In_ Compositor const& compositor,
            _In_ DeckOverlay const& overlay,
            _In_ float width,
            _In_ float height,
            _In_ double scale)
        {
            auto const pitch = static_cast<float>(
                std::max(1.0, overlay.ScanLinePitch / std::max(scale, 0.05)));

            auto const thickness = static_cast<float>(
                std::max(0.5, ScanLineThicknessPixels / std::max(scale, 0.05)));

            auto const count = static_cast<int32_t>(height / pitch) + 1;

            auto lines = compositor.CreateShapeVisual();
            lines.Size(float2{ width, height });

            auto line = overlay.ScanLineColor;
            line.A = static_cast<uint8_t>(
                std::clamp(std::lround(255.0 * overlay.ScanLineStrength / 100.0), 0L, 255L));

            auto const brush = compositor.CreateColorBrush(ToColor(line));

            for (int32_t i = 0; i < std::min(count, MaximumScanLines); ++i)
            {
                auto geometry = compositor.CreateRectangleGeometry();
                geometry.Size(float2{ width, thickness });
                geometry.Offset(float2{ 0.0f, i * pitch });

                auto shape = compositor.CreateSpriteShape(geometry);
                shape.FillBrush(brush);

                lines.Shapes().Append(shape);
            }

            return lines;
        }

        Visual BuildVignette(
            _In_ Compositor const& compositor,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height)
        {
            auto corner = EffectiveVignetteColor(theme);

            auto clear = corner;
            clear.A = 0;

            auto knee = corner;
            knee.A = static_cast<uint8_t>(
                std::clamp(std::lround(255.0 * theme.Overlay.VignettePercent / 100.0 * 0.5), 0L, 255L));

            corner.A = static_cast<uint8_t>(
                std::clamp(std::lround(255.0 * theme.Overlay.VignettePercent / 100.0), 0L, 255L));

            auto brush = compositor.CreateRadialGradientBrush();

            brush.EllipseCenter(float2{ 0.5f, VignetteCenterY });
            brush.EllipseRadius(float2{ VignetteRadiusX, VignetteRadiusY });

            struct Stop { float Offset; ThemeColor Color; };

            Stop const stops[]
            {
                { VignetteClearTo, clear },
                { VignetteKnee, knee },
                { 1.0f, corner },
            };

            for (auto const& stop : stops)
            {
                auto gradientStop = compositor.CreateColorGradientStop();
                gradientStop.Offset(stop.Offset);
                gradientStop.Color(ToColor(stop.Color));

                brush.ColorStops().Append(gradientStop);
            }

            auto sprite = compositor.CreateSpriteVisual();
            sprite.Size(float2{ width, height });
            sprite.Brush(brush);

            return sprite;
        }

        Visual BuildFaceplate(
            _In_ Compositor const& compositor,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height)
        {
            auto light = EffectiveFaceplateColor(theme);

            auto const peak = std::clamp(theme.Overlay.FaceplateSheenPercent / 100.0, 0.0, 1.0);

            auto clear = light;
            clear.A = 0;

            auto mid = light;
            mid.A = static_cast<uint8_t>(std::clamp(std::lround(255.0 * peak * 0.36), 0L, 255L));

            light.A = static_cast<uint8_t>(std::clamp(std::lround(255.0 * peak), 0L, 255L));

            auto brush = compositor.CreateLinearGradientBrush();

            brush.StartPoint(float2{ 0.0f, 0.0f });
            brush.EndPoint(float2{ FaceplateRunX, FaceplateRunY });

            struct Stop { float Offset; ThemeColor Color; };

            Stop const stops[]
            {
                { 0.0f, light },
                { FaceplateKnee, mid },
                { FaceplateEnd, clear },
                { 1.0f, clear },
            };

            for (auto const& stop : stops)
            {
                auto gradientStop = compositor.CreateColorGradientStop();
                gradientStop.Offset(stop.Offset);
                gradientStop.Color(ToColor(stop.Color));

                brush.ColorStops().Append(gradientStop);
            }

            auto sprite = compositor.CreateSpriteVisual();
            sprite.Size(float2{ width, height });
            sprite.Brush(brush);

            return sprite;
        }
    }

    _Use_decl_annotations_
    media::Brush MakeDeckBrush(ThemeDeck const& deck)
    {
        if (deck.Kind != DeckKind::Gradient)
        {
            // An image deck is drawn over its color by the surface, so the color is right here
            // for that case too.
            return media::SolidColorBrush(ToColor(deck.Color));
        }

        media::RadialGradientBrush brush{};

        // Lit from above and slightly off the top edge, so the fall-off reaches the bottom
        // corners of a wide page instead of stopping a third of the way down.
        brush.MappingMode(media::BrushMappingMode::RelativeToBoundingBox);
        brush.Center({ 0.5f, -0.10f });
        brush.GradientOrigin({ 0.5f, -0.10f });
        brush.RadiusX(1.30f);
        brush.RadiusY(1.25f);

        media::GradientStop top{};
        top.Color(ToColor(deck.Color));
        top.Offset(0.0);

        // The turn happens well before the edge rather than evenly across it, which is what
        // gives a deck a floor instead of a vignette.
        media::GradientStop knee{};
        knee.Color(ToColor(BlendOver(deck.Color, deck.GradientEndColor, 0.55)));
        knee.Offset(0.58);

        media::GradientStop floor{};
        floor.Color(ToColor(deck.GradientEndColor));
        floor.Offset(1.0);

        brush.GradientStops().Append(top);
        brush.GradientStops().Append(knee);
        brush.GradientStops().Append(floor);

        return brush;
    }

    _Use_decl_annotations_
    void ApplyDeckOverlay(
        xaml::UIElement const& element,
        Theme const& theme,
        double width,
        double height,
        double scale,
        DeckOverlayLayer layer)
    {
        if (element == nullptr)
        {
            return;
        }

        try
        {
            auto const wantsGrain = layer != DeckOverlayLayer::AboveControls &&
                theme.Overlay.GrainPercent > 0;

            auto const wantsGlass = layer != DeckOverlayLayer::BeneathControls;

            auto const wantsVignette = wantsGlass && theme.Overlay.VignettePercent > 0;

            auto const wantsScanLines = wantsGlass &&
                theme.Overlay.ScanLinePitch > 0 && theme.Overlay.ScanLineStrength > 0;

            auto const wantsFaceplate = wantsGlass && theme.Overlay.FaceplateSheenPercent > 0;

            if ((!wantsGrain && !wantsVignette && !wantsScanLines && !wantsFaceplate) ||
                width < 1.0 || height < 1.0)
            {
                ElementCompositionPreview::SetElementChildVisual(element, nullptr);
                return;
            }

            auto const compositor = ElementCompositionPreview::GetElementVisual(element).Compositor();

            auto const pixelWidth = static_cast<float>(width);
            auto const pixelHeight = static_cast<float>(height);

            auto root = compositor.CreateContainerVisual();
            root.Size(float2{ pixelWidth, pixelHeight });

            // Bottom to top, and this order is the design's: the panel has its texture, the
            // glass falls off at the corners, the raster is drawn on it, and the room is
            // reflected in front of all three.
            if (wantsGrain)
            {
                root.Children().InsertAtTop(BuildGrain(compositor, theme, pixelWidth, pixelHeight));
            }

            if (wantsVignette)
            {
                root.Children().InsertAtTop(BuildVignette(compositor, theme, pixelWidth, pixelHeight));
            }

            if (wantsScanLines)
            {
                root.Children().InsertAtTop(
                    BuildScanLines(compositor, theme.Overlay, pixelWidth, pixelHeight, scale));
            }

            if (wantsFaceplate)
            {
                root.Children().InsertAtTop(BuildFaceplate(compositor, theme, pixelWidth, pixelHeight));
            }

            ElementCompositionPreview::SetElementChildVisual(element, root);
        }
        catch (...)
        {
            try
            {
                ElementCompositionPreview::SetElementChildVisual(element, nullptr);
            }
            catch (...)
            {
            }
        }
    }
}
