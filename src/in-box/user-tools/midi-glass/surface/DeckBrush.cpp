// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "DeckBrush.h"
#include "SurfaceColors.h"
#include "SurfaceTextures.h"
#include "ThemeStore.h"

#include <winrt/Windows.UI.ViewManagement.h>

#include <algorithm>
#include <cmath>
#include <vector>

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

        // Brushed metal is every row lit a little differently, in a handful of shades either
        // side of the average. The step is measured off the Airy System comp, where one row
        // differs from the next by about two and a half levels on a deck near thirty.
        constexpr int32_t BrushedShadeCount = 7;
        constexpr double BrushedShadeStep = 0.09;

        // A brushed grain with no run length of its own named streaks this many cells long.
        constexpr int32_t DefaultBrushedStreak = 12;

        // The rain: a tile of fine streaks nine degrees off the vertical, repeated. It falls one
        // tile across and six down per loop, which is that same nine degrees, so the loop meets
        // itself without a seam.
        constexpr float RainTileSize = 240.0f;
        constexpr int32_t RainStreakCount = 54;
        constexpr double RainSlantDegrees = 9.0;
        constexpr int32_t RainTilesPerLoop = 6;
        constexpr uint64_t RainSeed = 20190101ull;

        // Deterministic, so a re-render is the same grain rather than a shimmer. A real random
        // source here would make every resize look like the panel was re-manufactured.
        uint32_t NextGrainNoise(_Inout_ uint32_t& state) noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            return state;
        }

        // The rain's own generator, so a tile is the same streaks every time it is drawn.
        double NextRainUnit(_Inout_ uint64_t& state) noexcept
        {
            state = (state * 1103515245ull + 12345ull) % 2147483648ull;

            return static_cast<double>(state) / 2147483648.0;
        }

        // A picture repeated at its own size from the top left of the page, at the page's scale.
        Visual BuildRepeatingPicture(
            _In_ Compositor const& compositor,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height,
            _In_ double pageScale)
        {
            auto const pixels = LoadThemePicture(theme.Deck.ImageFileName);

            if (pixels == nullptr)
            {
                return nullptr;
            }

            auto const scale = static_cast<float>(std::max(pageScale, 0.05));

            auto tiles = BuildTiles(
                compositor,
                MakeTextureBrush(compositor, *pixels),
                static_cast<float>(pixels->Width) * scale,
                static_cast<float>(pixels->Height) * scale,
                width,
                height);

            tiles.Clip(compositor.CreateInsetClip());

            return tiles;
        }

        // A fine noise in every screen pixel, whatever the page's zoom.
        Visual BuildFineGrain(
            _In_ Compositor const& compositor,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height,
            _In_ double scale)
        {
            auto const pixels = FineGrainPixels(theme);

            if (pixels == nullptr)
            {
                return nullptr;
            }

            auto const unitsPerPixel = static_cast<float>(1.0 / std::max(scale, 0.05));

            auto tiles = BuildTiles(
                compositor,
                MakeTextureBrush(compositor, *pixels),
                static_cast<float>(pixels->Width) * unitsPerPixel,
                static_cast<float>(pixels->Height) * unitsPerPixel,
                width,
                height);

            tiles.Clip(compositor.CreateInsetClip());

            return tiles;
        }

        // Rain, falling where the page's animations are allowed to move.
        Visual BuildRain(
            _In_ Compositor const& compositor,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height,
            _In_ double pageScale,
            _In_ bool animate)
        {
            auto const scale = static_cast<float>(std::max(pageScale, 0.05));
            auto const tileSize = RainTileSize * scale;
            auto const strength = std::clamp(theme.Overlay.RainPercent, 0, 100) / 100.0;

            auto color = EffectiveRainColor(theme);

            auto tile = compositor.CreateShapeVisual();
            tile.Size(float2{ tileSize, tileSize });

            auto const slant = std::tan(RainSlantDegrees * 3.14159265358979 / 180.0);

            uint64_t state = RainSeed;

            for (int32_t streak = 0; streak < RainStreakCount; ++streak)
            {
                auto const x = NextRainUnit(state) * RainTileSize;
                auto const y = NextRainUnit(state) * RainTileSize;
                auto const length = 12.0 + NextRainUnit(state) * 26.0;
                auto const alpha = (0.20 + NextRainUnit(state) * 0.34) * strength;
                auto const thickness = NextRainUnit(state) < 0.25 ? 1.3 : 0.9;

                auto const x2 = x + length * slant;
                auto const y2 = y + length;

                color.A = static_cast<uint8_t>(std::clamp(std::lround(255.0 * alpha), 0L, 255L));

                auto const brush = compositor.CreateColorBrush(ToColor(color));

                // A streak that crosses the edge of the tile is drawn again on the far side, so
                // the tile repeats without a seam.
                for (auto const dx : { -RainTileSize, 0.0f, RainTileSize })
                {
                    for (auto const dy : { -RainTileSize, 0.0f, RainTileSize })
                    {
                        auto const ax = x + dx;
                        auto const ay = y + dy;
                        auto const bx = x2 + dx;
                        auto const by = y2 + dy;

                        if (std::max(ax, bx) < 0.0 || std::min(ax, bx) > RainTileSize ||
                            std::max(ay, by) < 0.0 || std::min(ay, by) > RainTileSize)
                        {
                            continue;
                        }

                        auto line = compositor.CreateLineGeometry();
                        line.Start(float2{ static_cast<float>(ax) * scale, static_cast<float>(ay) * scale });
                        line.End(float2{ static_cast<float>(bx) * scale, static_cast<float>(by) * scale });

                        auto shape = compositor.CreateSpriteShape(line);
                        shape.StrokeBrush(brush);
                        shape.StrokeThickness(static_cast<float>(thickness) * scale);
                        shape.StrokeStartCap(CompositionStrokeCap::Round);
                        shape.StrokeEndCap(CompositionStrokeCap::Round);

                        tile.Shapes().Append(shape);
                    }
                }
            }

            tile.Clip(compositor.CreateInsetClip());

            auto surface = compositor.CreateVisualSurface();
            surface.SourceVisual(tile);
            surface.SourceSize(float2{ tileSize, tileSize });

            auto const brush = compositor.CreateSurfaceBrush(surface);

            // Tiles enough to cover the page from one loop's start to its end.
            auto const loopAcross = tileSize;
            auto const loopDown = tileSize * static_cast<float>(RainTilesPerLoop);

            auto field = BuildTiles(compositor, brush, tileSize, tileSize, width + loopAcross, height + loopDown);

            // The offscreen is only rendered while the visual it reads from is alive, so the
            // tile goes along with the field, hidden.
            tile.IsVisible(false);
            field.Children().InsertAtBottom(tile);

            field.Offset(float3{ -loopAcross, -loopDown, 0.0f });

            auto root = compositor.CreateContainerVisual();
            root.Size(float2{ width, height });
            root.Clip(compositor.CreateInsetClip());
            root.Children().InsertAtTop(field);

            auto const speed = std::clamp(theme.Overlay.RainSpeed, 0, 4000);

            auto moves = animate && speed > 0;

            if (moves)
            {
                try
                {
                    moves = winrt::Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled();
                }
                catch (...)
                {
                    moves = false;
                }
            }

            if (moves)
            {
                auto const seconds = (RainTileSize * RainTilesPerLoop) / static_cast<float>(speed);

                auto fall = compositor.CreateVector3KeyFrameAnimation();
                fall.InsertKeyFrame(0.0f, float3{ -loopAcross, -loopDown, 0.0f }, compositor.CreateLinearEasingFunction());
                fall.InsertKeyFrame(1.0f, float3{ 0.0f, 0.0f, 0.0f }, compositor.CreateLinearEasingFunction());
                fall.Duration(std::chrono::milliseconds{ static_cast<int64_t>(seconds * 1000.0f) });
                fall.IterationBehavior(AnimationIterationBehavior::Forever);

                field.StartAnimation(L"Offset", fall);
            }

            return root;
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

            if (theme.Overlay.Grain == GrainStyle::Brushed)
            {
                // Brushed rather than sanded: one pixel rows, each a shade of the grain color,
                // in runs about as long as the theme asks. Always lighter, never darker, the way
                // light catches the ridges of a brushed panel.
                auto const averageAlpha = 255.0 * strength * 0.16;

                std::vector<CompositionColorBrush> shades{};
                shades.reserve(BrushedShadeCount);

                for (int32_t level = 0; level < BrushedShadeCount; ++level)
                {
                    auto streak = speckle;
                    streak.A = static_cast<uint8_t>(std::clamp(
                        std::lround(averageAlpha * (1.0 + BrushedShadeStep * (level - BrushedShadeCount / 2))),
                        0L,
                        255L));

                    shades.push_back(compositor.CreateColorBrush(ToColor(streak)));
                }

                auto const cellsPerStreak = theme.Overlay.GrainStreak > 1 ? theme.Overlay.GrainStreak : DefaultBrushedStreak;
                auto const typicalRun = static_cast<float>(cellsPerStreak) * GrainCellSize;
                auto const rows = static_cast<int32_t>(GrainTileSize);

                for (int32_t row = 0; row < rows; ++row)
                {
                    auto const first = static_cast<int32_t>(NextGrainNoise(state) % BrushedShadeCount);

                    auto level = first;
                    auto x = 0.0f;

                    while (x < GrainTileSize)
                    {
                        auto const roll = NextGrainNoise(state);

                        // Half to one and a half times the typical run.
                        auto run = typicalRun * (0.5f + static_cast<float>(roll % 1000u) / 1000.0f);

                        // A row ends on the shade it started with, so the tile repeats across the
                        // page without a seam every tile's width.
                        if (x + run >= GrainTileSize)
                        {
                            run = GrainTileSize - x;
                            level = first;
                        }

                        auto geometry = compositor.CreateRectangleGeometry();
                        geometry.Size(float2{ run, 1.0f });
                        geometry.Offset(float2{ x, static_cast<float>(row) });

                        auto shape = compositor.CreateSpriteShape(geometry);
                        shape.FillBrush(shades[static_cast<size_t>(level)]);

                        tile.Shapes().Append(shape);

                        x += run;

                        // The next run along is a shade either side of this one at most, which is
                        // what keeps a row reading as one streak rather than as dashes.
                        level = std::clamp(level + static_cast<int32_t>((roll >> 16) % 3u) - 1, 0, BrushedShadeCount - 1);
                    }
                }
            }
            else
            {
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
        // A picture that repeats is laid over the deck's colors by the overlay, a tile at a time;
        // one stretched to the page is the whole deck.
        if (deck.Kind == DeckKind::Image && !deck.ImageRepeats)
        {
            // A missing or unreadable picture leaves the deck its color, never a hole.
            if (auto const path = DeckImagePath(deck); !path.empty())
            {
                media::Imaging::BitmapImage bitmap{};
                bitmap.UriSource(winrt::Windows::Foundation::Uri{ L"file:///" + winrt::hstring{ path } });

                media::ImageBrush brush{};
                brush.ImageSource(bitmap);
                brush.Stretch(media::Stretch::UniformToFill);
                brush.AlignmentX(media::AlignmentX::Center);
                brush.AlignmentY(media::AlignmentY::Center);

                return brush;
            }
        }

        // Under a repeating picture, the deck is lit the way a gradient deck is when its two
        // colors differ.
        auto const lit = deck.Kind == DeckKind::Gradient ||
            (deck.Kind == DeckKind::Image && deck.ImageRepeats && !(deck.GradientEndColor == deck.Color));

        if (!lit)
        {
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
        DeckOverlayLayer layer,
        double pageScale,
        bool animate)
    {
        if (element == nullptr)
        {
            return;
        }

        try
        {
            auto const beneath = layer != DeckOverlayLayer::AboveControls;

            auto const wantsPicture = beneath &&
                theme.Deck.Kind == DeckKind::Image &&
                theme.Deck.ImageRepeats &&
                !theme.Deck.ImageFileName.empty();

            auto const wantsGrain = beneath && theme.Overlay.GrainPercent > 0;

            auto const wantsRain = beneath && theme.Overlay.RainPercent > 0;

            auto const wantsGlass = layer != DeckOverlayLayer::BeneathControls;

            auto const wantsVignette = wantsGlass && theme.Overlay.VignettePercent > 0;

            auto const wantsScanLines = wantsGlass &&
                theme.Overlay.ScanLinePitch > 0 && theme.Overlay.ScanLineStrength > 0;

            auto const wantsFaceplate = wantsGlass && theme.Overlay.FaceplateSheenPercent > 0;

            if ((!wantsPicture && !wantsGrain && !wantsRain && !wantsVignette && !wantsScanLines && !wantsFaceplate) ||
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

            // Bottom to top, and this order is the design's: the wall's picture, the panel's
            // texture, the rain running down it, then the glass falls off at the corners, the
            // raster is drawn on it, and the room is reflected in front of all of it.
            if (wantsPicture)
            {
                if (auto picture = BuildRepeatingPicture(compositor, theme, pixelWidth, pixelHeight, pageScale); picture != nullptr)
                {
                    root.Children().InsertAtTop(picture);
                }
            }

            if (wantsGrain)
            {
                if (theme.Overlay.Grain == GrainStyle::Fine)
                {
                    if (auto fine = BuildFineGrain(compositor, theme, pixelWidth, pixelHeight, scale); fine != nullptr)
                    {
                        root.Children().InsertAtTop(fine);
                    }
                }
                else
                {
                    root.Children().InsertAtTop(BuildGrain(compositor, theme, pixelWidth, pixelHeight));
                }
            }

            if (wantsRain)
            {
                root.Children().InsertAtTop(BuildRain(compositor, theme, pixelWidth, pixelHeight, pageScale, animate));
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
