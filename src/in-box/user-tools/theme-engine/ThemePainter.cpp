// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThemePainter.h"
#include "ThemeXaml.h"

#include <cmath>
#include <format>

#include <winrt/Microsoft.Graphics.Canvas.Effects.h>

namespace themeengine
{
    namespace
    {
        namespace wgc = ::winrt::Microsoft::Graphics::Canvas;
        namespace wgcb = ::winrt::Microsoft::Graphics::Canvas::Brushes;
        namespace wgcg = ::winrt::Microsoft::Graphics::Canvas::Geometry;
        namespace wgce = ::winrt::Microsoft::Graphics::Canvas::Effects;
        namespace wf = ::winrt::Windows::Foundation;
        using float2 = ::winrt::Windows::Foundation::Numerics::float2;

        constexpr double Pi = 3.14159265358979323846;

        // Shapes are drawn once into images this many keep, then the oldest go.
        constexpr size_t MaximumShadowImages = 512;

        // Images up to this size are kept whole for one shape, rather than sliced to stretch.
        constexpr float MaximumWholeImage = 160.0f;

        wf::Rect ToRect(PaintRect const& rect) noexcept
        {
            return { rect.X, rect.Y, rect.Width, rect.Height };
        }

        PaintRect Inflate(PaintRect const& rect, float amount) noexcept
        {
            return { rect.X - amount, rect.Y - amount, rect.Width + amount * 2.0f, rect.Height + amount * 2.0f };
        }

        std::array<float, 4> CornerRadii(ResolvedStyle const& style, PaintRect const& rect) noexcept
        {
            std::array<float, 4> radii{};

            for (size_t i = 0; i < radii.size(); ++i)
            {
                radii[i] = static_cast<float>(style.CornerRadius(i, rect.Width, rect.Height));
            }

            return radii;
        }

        // A rounded rectangle with each corner's own radius.
        wgcg::CanvasGeometry RoundedShape(wgc::ICanvasResourceCreator const& creator, PaintRect const& rect, std::array<float, 4> const& radii)
        {
            auto const w = std::max(0.0f, rect.Width);
            auto const h = std::max(0.0f, rect.Height);

            if (radii[0] == radii[1] && radii[1] == radii[2] && radii[2] == radii[3])
            {
                return wgcg::CanvasGeometry::CreateRoundedRectangle(creator, rect.X, rect.Y, w, h, radii[0], radii[0]);
            }

            auto const x = rect.X;
            auto const y = rect.Y;
            auto const [tl, tr, br, bl] = radii;

            wgcg::CanvasPathBuilder path{ creator };
            path.BeginFigure(float2{ x + tl, y });
            path.AddLine(float2{ x + w - tr, y });

            if (tr > 0) { path.AddArc(float2{ x + w, y + tr }, tr, tr, 0.0f, wgcg::CanvasSweepDirection::Clockwise, wgcg::CanvasArcSize::Small); }

            path.AddLine(float2{ x + w, y + h - br });

            if (br > 0) { path.AddArc(float2{ x + w - br, y + h }, br, br, 0.0f, wgcg::CanvasSweepDirection::Clockwise, wgcg::CanvasArcSize::Small); }

            path.AddLine(float2{ x + bl, y + h });

            if (bl > 0) { path.AddArc(float2{ x, y + h - bl }, bl, bl, 0.0f, wgcg::CanvasSweepDirection::Clockwise, wgcg::CanvasArcSize::Small); }

            path.AddLine(float2{ x, y + tl });

            if (tl > 0) { path.AddArc(float2{ x + tl, y }, tl, tl, 0.0f, wgcg::CanvasSweepDirection::Clockwise, wgcg::CanvasArcSize::Small); }

            path.EndFigure(wgcg::CanvasFigureLoop::Closed);
            return wgcg::CanvasGeometry::CreatePath(path);
        }

        // CSS grows a corner by the spread (or shrinks it) only where it's already round.
        std::array<float, 4> SpreadRadii(std::array<float, 4> radii, float spread) noexcept
        {
            for (auto& radius : radii)
            {
                radius = radius > 0 ? std::max(0.0f, radius + spread) : 0.0f;
            }

            return radii;
        }

        std::wstring RadiiKey(std::array<float, 4> const& radii)
        {
            return std::format(L"{:.2f},{:.2f},{:.2f},{:.2f}", radii[0], radii[1], radii[2], radii[3]);
        }

        wgcg::CanvasStrokeStyle StrokeFor(LineStyle style)
        {
            wgcg::CanvasStrokeStyle stroke{};

            if (style == LineStyle::Dashed)
            {
                float const dashes[]{ 3.0f, 2.0f };
                stroke.CustomDashStyle(dashes);
            }
            else if (style == LineStyle::Dotted)
            {
                float const dots[]{ 1.0f, 1.0f };
                stroke.CustomDashStyle(dots);
            }

            return stroke;
        }

        bool IsOpaqueBackground(ResolvedStyle const& style) noexcept
        {
            for (auto const& layer : style.Background)
            {
                if (layer.Kind == BackgroundKind::Solid && layer.Solid.A >= 0.999)
                {
                    return true;
                }

                if ((layer.Kind == BackgroundKind::Linear || layer.Kind == BackgroundKind::Radial) && !layer.Stops.empty() &&
                    std::all_of(layer.Stops.begin(), layer.Stops.end(), [](ResolvedStop const& stop) { return stop.Value.A >= 0.999; }))
                {
                    return true;
                }
            }

            return false;
        }
    }

    _Use_decl_annotations_
    winrt::Windows::UI::Color FilteredColor(Color const& color, FilterValue const& filter) noexcept
    {
        return ToWindowsColor(ApplyFilter(color, filter));
    }

    _Use_decl_annotations_
    PaintRect PartRect(ResolvedStyle const& style, PaintRect const& space) noexcept
    {
        PaintRect rect{
            space.X + static_cast<float>(style.Margin[3]),
            space.Y + static_cast<float>(style.Margin[0]),
            space.Width - static_cast<float>(style.Margin[1] + style.Margin[3]),
            space.Height - static_cast<float>(style.Margin[0] + style.Margin[2]) };

        if (style.Width.has_value() && *style.Width < rect.Width)
        {
            rect.X += (rect.Width - static_cast<float>(*style.Width)) / 2.0f;
            rect.Width = static_cast<float>(*style.Width);
        }

        if (style.Height.has_value() && *style.Height < rect.Height)
        {
            rect.Y += (rect.Height - static_cast<float>(*style.Height)) / 2.0f;
            rect.Height = static_cast<float>(*style.Height);
        }

        rect.Width = std::max(0.0f, rect.Width);
        rect.Height = std::max(0.0f, rect.Height);
        return rect;
    }

    _Use_decl_annotations_
    wgcg::CanvasGeometry PartShape(wgc::ICanvasResourceCreator const& creator, ResolvedStyle const& style, PaintRect const& rect, float inflate)
    {
        return RoundedShape(creator, Inflate(rect, inflate), SpreadRadii(CornerRadii(style, rect), inflate));
    }

    void ThemePainter::Reset() noexcept
    {
        m_shadows.clear();
    }

    _Use_decl_annotations_
    wgcb::ICanvasBrush ThemePainter::BackgroundBrush(wgc::ICanvasResourceCreator const& creator, ResolvedBackground const& layer, FilterValue const& filter, PaintRect const& rect)
    {
        if (layer.Kind == BackgroundKind::Solid || layer.Stops.empty())
        {
            return wgcb::CanvasSolidColorBrush{ creator, FilteredColor(layer.Solid, filter) };
        }

        std::vector<wgcb::CanvasGradientStop> stops{};
        stops.reserve(layer.Stops.size());

        for (auto const& stop : layer.Stops)
        {
            stops.push_back({ static_cast<float>(std::clamp(stop.Position, 0.0, 1.0)), FilteredColor(stop.Value, filter) });
        }

        auto const w = static_cast<double>(rect.Width);
        auto const h = static_cast<double>(rect.Height);

        if (layer.Kind == BackgroundKind::Linear)
        {
            // CSS: through the middle at the angle, just long enough to reach the corners.
            auto const radians = layer.Angle * Pi / 180.0;
            auto const dx = std::sin(radians);
            auto const dy = -std::cos(radians);
            auto const half = (std::abs(w * dx) + std::abs(h * dy)) / 2.0;
            auto const cx = rect.X + w / 2.0;
            auto const cy = rect.Y + h / 2.0;

            wgcb::CanvasLinearGradientBrush brush{ creator, stops };
            brush.StartPoint(float2{ static_cast<float>(cx - dx * half), static_cast<float>(cy - dy * half) });
            brush.EndPoint(float2{ static_cast<float>(cx + dx * half), static_cast<float>(cy + dy * half) });
            return brush;
        }

        auto const cx = layer.CenterX * w;
        auto const cy = layer.CenterY * h;
        double rx{};
        double ry{};

        if (layer.SizeX.has_value() && layer.SizeY.has_value())
        {
            rx = *layer.SizeX * w;
            ry = *layer.SizeY * h;
        }
        else
        {
            auto const farX = std::max(cx, w - cx);
            auto const farY = std::max(cy, h - cy);

            if (layer.Circle)
            {
                rx = ry = std::sqrt(farX * farX + farY * farY);
            }
            else
            {
                // An ellipse that keeps closest-side's proportions and passes through the farthest corner.
                auto const nearX = std::max(1.0, std::min(cx, w - cx));
                auto const nearY = std::max(1.0, std::min(cy, h - cy));
                auto const scale = std::sqrt((farX / nearX) * (farX / nearX) + (farY / nearY) * (farY / nearY));
                rx = nearX * scale;
                ry = nearY * scale;
            }
        }

        wgcb::CanvasRadialGradientBrush brush{ creator, stops };
        brush.Center(float2{ static_cast<float>(rect.X + cx), static_cast<float>(rect.Y + cy) });
        brush.RadiusX(static_cast<float>(std::max(rx, 0.5)));
        brush.RadiusY(static_cast<float>(std::max(ry, 0.5)));
        return brush;
    }

    _Use_decl_annotations_
    ThemePainter::NineSlice const* ThemePainter::OuterShadow(
        wgc::ICanvasResourceCreatorWithDpi const& creator,
        ResolvedStyle const& style,
        ResolvedShadow const& shadow,
        PaintRect const& rect)
    {
        auto const spread = static_cast<float>(shadow.Spread);
        auto const radii = SpreadRadii(CornerRadii(style, rect), spread);
        auto const sigma = static_cast<float>(shadow.Blur / 2.0);
        auto const reach = std::ceil(sigma * 3.0f);
        auto const corner = std::max(1.0f, std::ceil(*std::max_element(radii.begin(), radii.end())));

        auto const shapeWidth = rect.Width + spread * 2.0f;
        auto const shapeHeight = rect.Height + spread * 2.0f;

        if (shapeWidth <= 0 || shapeHeight <= 0)
        {
            return nullptr;
        }

        // A side shorter than its two corners can't be sliced; that way the image is kept whole,
        // at the shape's size.
        auto const wholeX = shapeWidth < corner * 2.0f + 1.0f;
        auto const wholeY = shapeHeight < corner * 2.0f + 1.0f;

        if ((wholeX && shapeWidth > MaximumWholeImage) || (wholeY && shapeHeight > MaximumWholeImage))
        {
            return nullptr;
        }

        auto const size = std::format(L"{}x{}", wholeX ? std::format(L"{:.2f}", shapeWidth) : std::wstring{}, wholeY ? std::format(L"{:.2f}", shapeHeight) : std::wstring{});
        auto const key = std::format(L"o|{}|{:.2f}|{:08X}|{}", RadiiKey(radii), shadow.Blur, ToWindowsColor(shadow.Value).A == 0 ? 0u : shadow.Value.ToArgb(), size);

        if (auto const found = m_shadows.find(key); found != m_shadows.end())
        {
            return &found->second;
        }

        auto const width = wholeX ? shapeWidth : corner * 2.0f + 1.0f;
        auto const height = wholeY ? shapeHeight : corner * 2.0f + 1.0f;

        wgc::CanvasRenderTarget image{ creator, width + reach * 2.0f, height + reach * 2.0f };
        auto const shape = RoundedShape(creator, { reach, reach, width, height }, radii);

        {
            auto ds = image.CreateDrawingSession();
            ds.Clear(winrt::Windows::UI::Colors::Transparent());

            if (sigma > 0)
            {
                wgc::CanvasCommandList list{ creator };

                {
                    auto listSession = list.CreateDrawingSession();
                    listSession.FillGeometry(shape, ToWindowsColor(shadow.Value));
                    listSession.Close();
                }

                wgce::GaussianBlurEffect blur{};
                blur.Source(list);
                blur.BlurAmount(sigma);
                blur.BorderMode(wgce::EffectBorderMode::Soft);
                blur.Optimization(wgce::EffectOptimization::Quality);
                ds.DrawImage(blur);
            }
            else
            {
                ds.FillGeometry(shape, ToWindowsColor(shadow.Value));
            }

            ds.Close();
        }

        if (m_shadows.size() >= MaximumShadowImages)
        {
            m_shadows.clear();
        }

        return &m_shadows.emplace(key, NineSlice{ image, corner, reach, wholeX, wholeY }).first->second;
    }

    _Use_decl_annotations_
    ThemePainter::NineSlice const* ThemePainter::InsetShadow(
        wgc::ICanvasResourceCreatorWithDpi const& creator,
        ResolvedStyle const& style,
        ResolvedShadow const& shadow,
        PaintRect const& rect)
    {
        auto const radii = CornerRadii(style, rect);
        auto const spread = static_cast<float>(shadow.Spread);
        auto const sigma = static_cast<float>(shadow.Blur / 2.0);
        auto const offsetX = static_cast<float>(shadow.X);
        auto const offsetY = static_cast<float>(shadow.Y);

        // Far enough in from every edge that the shadow there is the same all along the edge.
        auto const corner = std::ceil(*std::max_element(radii.begin(), radii.end()) + sigma * 3.0f +
            std::abs(offsetX) + std::abs(offsetY) + std::abs(spread)) + 1.0f;

        auto const wholeX = rect.Width < corner * 2.0f + 1.0f;
        auto const wholeY = rect.Height < corner * 2.0f + 1.0f;

        if ((wholeX && rect.Width > MaximumWholeImage) || (wholeY && rect.Height > MaximumWholeImage))
        {
            return nullptr;
        }

        auto const size = std::format(L"{}x{}", wholeX ? std::format(L"{:.2f}", rect.Width) : std::wstring{}, wholeY ? std::format(L"{:.2f}", rect.Height) : std::wstring{});
        auto const key = std::format(L"i|{}|{:.2f}|{:.2f}|{:.2f}|{:.2f}|{:08X}|{}", RadiiKey(radii), shadow.Blur, offsetX, offsetY, spread, shadow.Value.ToArgb(), size);

        if (auto const found = m_shadows.find(key); found != m_shadows.end())
        {
            return &found->second;
        }

        auto const width = wholeX ? rect.Width : corner * 2.0f + 1.0f;
        auto const height = wholeY ? rect.Height : corner * 2.0f + 1.0f;

        wgc::CanvasRenderTarget image{ creator, width, height };
        auto const outline = RoundedShape(creator, { 0, 0, width, height }, radii);

        // The shadow is everything outside the hole the light leaves, blurred and kept inside.
        auto const hole = RoundedShape(creator, { spread + offsetX, spread + offsetY, width - spread * 2.0f, height - spread * 2.0f }, SpreadRadii(radii, -spread));
        auto const around = wgcg::CanvasGeometry::CreateRectangle(creator, -width - 64.0f, -height - 64.0f, width * 3.0f + 128.0f, height * 3.0f + 128.0f);
        auto const frame = around.CombineWith(hole, winrt::Windows::Foundation::Numerics::float3x2::identity(), wgcg::CanvasGeometryCombine::Exclude);

        {
            auto ds = image.CreateDrawingSession();
            ds.Clear(winrt::Windows::UI::Colors::Transparent());

            auto layer = ds.CreateLayer(1.0f, outline);

            if (sigma > 0)
            {
                wgc::CanvasCommandList list{ creator };

                {
                    auto listSession = list.CreateDrawingSession();
                    listSession.FillGeometry(frame, ToWindowsColor(shadow.Value));
                    listSession.Close();
                }

                wgce::GaussianBlurEffect blur{};
                blur.Source(list);
                blur.BlurAmount(sigma);
                blur.BorderMode(wgce::EffectBorderMode::Soft);
                blur.Optimization(wgce::EffectOptimization::Quality);
                ds.DrawImage(blur);
            }
            else
            {
                ds.FillGeometry(frame, ToWindowsColor(shadow.Value));
            }

            layer.Close();
            ds.Close();
        }

        if (m_shadows.size() >= MaximumShadowImages)
        {
            m_shadows.clear();
        }

        return &m_shadows.emplace(key, NineSlice{ image, corner, 0.0f, wholeX, wholeY }).first->second;
    }

    _Use_decl_annotations_
    void ThemePainter::DrawSlices(wgc::CanvasDrawingSession const& ds, NineSlice const& slices, PaintRect const& shape)
    {
        auto const& image = slices.Image;
        auto const reach = slices.Reach;

        if (slices.WholeX && slices.WholeY)
        {
            ds.DrawImage(image, wf::Rect{ shape.X - reach, shape.Y - reach, shape.Width + reach * 2.0f, shape.Height + reach * 2.0f });
            return;
        }

        auto const imageSize = image.Size();
        auto const c = slices.Corner;

        // Source: corner + reach, the one middle line, corner + reach. A side kept whole is one
        // piece, the shape's own size.
        float const sx[]{ 0.0f, c + reach, c + reach + 1.0f, static_cast<float>(imageSize.Width) };
        float const sy[]{ 0.0f, c + reach, c + reach + 1.0f, static_cast<float>(imageSize.Height) };

        float const dx[]{ shape.X - reach, shape.X + c, shape.X + shape.Width - c, shape.X + shape.Width + reach };
        float const dy[]{ shape.Y - reach, shape.Y + c, shape.Y + shape.Height - c, shape.Y + shape.Height + reach };

        float const wx[]{ 0.0f, static_cast<float>(imageSize.Width) };
        float const wy[]{ 0.0f, static_cast<float>(imageSize.Height) };
        float const vx[]{ shape.X - reach, shape.X + shape.Width + reach };
        float const vy[]{ shape.Y - reach, shape.Y + shape.Height + reach };

        auto const* const srcX = slices.WholeX ? wx : sx;
        auto const* const srcY = slices.WholeY ? wy : sy;
        auto const* const dstX = slices.WholeX ? vx : dx;
        auto const* const dstY = slices.WholeY ? vy : dy;
        size_t const columns = slices.WholeX ? 1 : 3;
        size_t const rows = slices.WholeY ? 1 : 3;

        for (size_t row = 0; row < rows; ++row)
        {
            for (size_t column = 0; column < columns; ++column)
            {
                auto const destinationWidth = dstX[column + 1] - dstX[column];
                auto const destinationHeight = dstY[row + 1] - dstY[row];

                if (destinationWidth <= 0 || destinationHeight <= 0)
                {
                    continue;
                }

                ds.DrawImage(image,
                    wf::Rect{ dstX[column], dstY[row], destinationWidth, destinationHeight },
                    wf::Rect{ srcX[column], srcY[row], srcX[column + 1] - srcX[column], srcY[row + 1] - srcY[row] },
                    1.0f,
                    wgc::CanvasImageInterpolation::Linear);
            }
        }
    }

    _Use_decl_annotations_
    void ThemePainter::Paint(
        wgc::CanvasDrawingSession const& ds,
        wgc::ICanvasResourceCreatorWithDpi const& creator,
        ResolvedStyle const& style,
        PaintRect const& rect,
        bool withOpacity)
    {
        if (rect.Width <= 0 || rect.Height <= 0 || !style.HasPlate())
        {
            return;
        }

        if (creator.Device() != m_device || creator.Dpi() != m_dpi)
        {
            Reset();
            m_device = creator.Device();
            m_dpi = creator.Dpi();
        }

        wgc::CanvasActiveLayer fade{ nullptr };

        if (withOpacity && style.Opacity < 1.0)
        {
            fade = ds.CreateLayer(static_cast<float>(style.Opacity));
        }

        auto const shape = PartShape(creator, style, rect);
        auto const& filter = style.Filter;

        // Outer shadows, the last one listed at the bottom as in CSS. They don't show through a
        // see-through background, as CSS clips them to outside the box.
        {
            wgc::CanvasActiveLayer outside{ nullptr };

            if (!IsOpaqueBackground(style))
            {
                auto const around = wgcg::CanvasGeometry::CreateRectangle(creator, rect.X - 512.0f, rect.Y - 512.0f, rect.Width + 1024.0f, rect.Height + 1024.0f);
                outside = ds.CreateLayer(1.0f, around.CombineWith(shape, winrt::Windows::Foundation::Numerics::float3x2::identity(), wgcg::CanvasGeometryCombine::Exclude));
            }

            for (auto shadow = style.Shadow.rbegin(); shadow != style.Shadow.rend(); ++shadow)
            {
                if (shadow->Inset || shadow->Value.A <= 0)
                {
                    continue;
                }

                auto filtered = *shadow;
                filtered.Value = ApplyFilter(shadow->Value, filter);

                if (auto const slices = OuterShadow(creator, style, filtered, rect))
                {
                    auto const spread = static_cast<float>(shadow->Spread);
                    PaintRect const shadowRect{ rect.X + static_cast<float>(shadow->X) - spread, rect.Y + static_cast<float>(shadow->Y) - spread,
                        rect.Width + spread * 2.0f, rect.Height + spread * 2.0f };

                    DrawSlices(ds, *slices, shadowRect);
                }
            }

            if (outside != nullptr)
            {
                outside.Close();
            }
        }

        // Backgrounds, the last layer at the bottom.
        for (auto layer = style.Background.rbegin(); layer != style.Background.rend(); ++layer)
        {
            if (layer->Kind == BackgroundKind::None)
            {
                continue;
            }

            ds.FillGeometry(shape, BackgroundBrush(creator, *layer, filter, rect));
        }

        // Inset shadows, inside the box over its background.
        for (auto shadow = style.Shadow.rbegin(); shadow != style.Shadow.rend(); ++shadow)
        {
            if (!shadow->Inset || shadow->Value.A <= 0)
            {
                continue;
            }

            auto filtered = *shadow;
            filtered.Value = ApplyFilter(shadow->Value, filter);

            if (auto const slices = InsetShadow(creator, style, filtered, rect))
            {
                DrawSlices(ds, *slices, rect);
            }
        }

        // The border, inside the box.
        auto const& border = style.Border;
        auto const uniform = border[0].IsVisible() && border[1].Width == border[0].Width && border[2].Width == border[0].Width &&
            border[3].Width == border[0].Width && border[1].Value == border[0].Value && border[2].Value == border[0].Value &&
            border[3].Value == border[0].Value && border[1].Style == border[0].Style && border[2].Style == border[0].Style && border[3].Style == border[0].Style;

        if (uniform)
        {
            auto const width = static_cast<float>(border[0].Width);
            auto const inner = PartShape(creator, style, rect, -width / 2.0f);
            ds.DrawGeometry(inner, FilteredColor(border[0].Value, filter), width, StrokeFor(border[0].Style));
        }
        else if (border[0].IsVisible() || border[1].IsVisible() || border[2].IsVisible() || border[3].IsVisible())
        {
            auto clip = ds.CreateLayer(1.0f, shape);

            if (border[0].IsVisible()) { ds.FillRectangle(rect.X, rect.Y, rect.Width, static_cast<float>(border[0].Width), FilteredColor(border[0].Value, filter)); }
            if (border[1].IsVisible()) { ds.FillRectangle(rect.X + rect.Width - static_cast<float>(border[1].Width), rect.Y, static_cast<float>(border[1].Width), rect.Height, FilteredColor(border[1].Value, filter)); }
            if (border[2].IsVisible()) { ds.FillRectangle(rect.X, rect.Y + rect.Height - static_cast<float>(border[2].Width), rect.Width, static_cast<float>(border[2].Width), FilteredColor(border[2].Value, filter)); }
            if (border[3].IsVisible()) { ds.FillRectangle(rect.X, rect.Y, static_cast<float>(border[3].Width), rect.Height, FilteredColor(border[3].Value, filter)); }

            clip.Close();
        }

        // The outline, outside the box (or inside, with a negative offset).
        if (style.Outline.IsVisible())
        {
            auto const width = static_cast<float>(style.Outline.Width);
            auto const ring = PartShape(creator, style, rect, static_cast<float>(style.OutlineOffset) + width / 2.0f);
            ds.DrawGeometry(ring, FilteredColor(style.Outline.Value, filter), width, StrokeFor(style.Outline.Style));
        }

        if (fade != nullptr)
        {
            fade.Close();
        }
    }
}
