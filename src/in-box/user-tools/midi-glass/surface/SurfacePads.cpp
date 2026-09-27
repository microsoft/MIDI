// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The note pads and the hex pads: a grid of pads that each play a note, colored by whether that
// note is in the key, with the pad under every finger lit.
//
// Where the pads go and what they play is worked out in the document layer (PadGrid), so what is
// drawn here and what a finger lands on can never disagree. This file only paints it.

#include "pch.h"
#include "SurfaceRenderer.h"
#include "PadGrid.h"

#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Microsoft.Graphics.Canvas.Geometry.h>

#include <cmath>
#include <limits>

using namespace winrt;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Windows::Foundation::Numerics;

namespace glass
{
    namespace
    {
        namespace canvas = ::winrt::Microsoft::Graphics::Canvas;
        namespace canvasGeometry = ::winrt::Microsoft::Graphics::Canvas::Geometry;

        // A square pad's corners follow the theme's, but never so far that a small pad turns
        // into a circle.
        constexpr float PadCornerFraction = 0.18f;

        // How far each corner of a hexagon is cut back to round it, as a fraction of its side.
        constexpr float HexCornerFraction = 0.16f;

        constexpr float PadRimThickness = 1.0f;

        // Where the light down the top of a pad has gone, and where the shade up from its bottom
        // starts: the same knee the renderer uses on a plate. The band between is the pad's flat
        // color, which is what its name was measured against.
        constexpr float PadSheenFalloff = 0.42f;

        // The most a pad darkens toward its bottom edge on a theme whose plates do.
        constexpr double MaximumPadFalloff = 0.18;

        // A pad's shadow is the theme's, but never wider than a quarter of the pad casting it.
        constexpr float PadShadowReach = 0.25f;

        // The name on a pad when the control does not give a size: a fraction of the pad, and
        // the least and most that comes to.
        constexpr double NoteNameFraction = 0.24;
        constexpr double MinimumNoteNameSize = 7.0;
        constexpr double MaximumAutomaticNoteNameSize = 18.0;

        // How far in from a square pad's edge a name at the top, the bottom or a corner sits.
        constexpr double NoteNameInset = 0.10;

        // The rectangle inside a hexagon a name can sit in, as fractions of the hexagon. Its
        // corners are cut away, so a name set in the top left of its bounding box would hang
        // off it.
        constexpr double HexNameBoxWidth = 0.72;
        constexpr double HexNameBoxHeight = 0.62;

        winrt::Windows::UI::Color ToWindowsColor(_In_ ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        // How much darker the theme's plates are at the bottom than at the top, as a fraction.
        double PlateFalloff(_In_ ControlColors const& colors) noexcept
        {
            if (colors.Plate.A == 0 || colors.Plate == colors.PlateEnd)
            {
                return 0.0;
            }

            auto const top = colors.Plate.R + colors.Plate.G + colors.Plate.B;
            auto const bottom = colors.PlateEnd.R + colors.PlateEnd.G + colors.PlateEnd.B;

            if (top <= bottom)
            {
                return 0.0;
            }

            return std::min(static_cast<double>(top - bottom) / static_cast<double>(top), MaximumPadFalloff);
        }

        // A pad's face, lit and shaded the way the theme lights its own pad control. Flat where
        // the theme is flat.
        CompositionBrush PadFaceBrush(
            _In_ Compositor const& compositor,
            _In_ ThemeColor const& face,
            _In_ ThemeColor const& sheen,
            _In_ ThemeColor const& shade,
            _In_ double falloff)
        {
            auto flat = face;
            flat.A = 255;

            if (sheen.A == 0 && shade.A == 0 && falloff <= 0.0)
            {
                return compositor.CreateColorBrush(ToWindowsColor(flat));
            }

            auto top = BlendOver(flat, sheen, 1.0);
            top.A = 255;

            auto bottom = BlendOver(BlendOver(flat, ThemeColor{ 0, 0, 0, 255 }, falloff), shade, 1.0);
            bottom.A = 255;

            struct Stop { float Offset; ThemeColor Color; };

            Stop const stops[]
            {
                { 0.0f, top },
                { PadSheenFalloff, flat },
                { 1.0f - PadSheenFalloff, flat },
                { 1.0f, bottom },
            };

            auto brush = compositor.CreateLinearGradientBrush();

            brush.StartPoint(float2{ 0.5f, 0.0f });
            brush.EndPoint(float2{ 0.5f, 1.0f });

            for (auto const& stop : stops)
            {
                auto gradientStop = compositor.CreateColorGradientStop();
                gradientStop.Offset(stop.Offset);
                gradientStop.Color(ToWindowsColor(stop.Color));

                brush.ColorStops().Append(gradientStop);
            }

            return brush;
        }

        // A hexagon standing on a point, in a box this wide and this tall, with every corner
        // rounded by the same cut.
        CompositionPath HexagonPath(_In_ float width, _In_ float height, _In_ float cornerCut)
        {
            float2 const corners[6]
            {
                { width * 0.5f, 0.0f },
                { width, height * 0.25f },
                { width, height * 0.75f },
                { width * 0.5f, height },
                { 0.0f, height * 0.75f },
                { 0.0f, height * 0.25f },
            };

            // Every side of a regular hexagon is as long as the distance from its middle to a
            // corner, which is half its height.
            auto const side = height * 0.5f;
            auto const cut = side > 0.0f ? std::clamp(cornerCut / side, 0.0f, 0.45f) : 0.0f;

            auto const along = [](float2 const& from, float2 const& to, float amount)
                {
                    return float2{ from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount };
                };

            canvasGeometry::CanvasPathBuilder builder{ canvas::CanvasDevice::GetSharedDevice() };

            for (size_t index = 0; index < 6; ++index)
            {
                auto const& corner = corners[index];
                auto const& previous = corners[(index + 5) % 6];
                auto const& next = corners[(index + 1) % 6];

                auto const into = along(corner, previous, cut);
                auto const outOf = along(corner, next, cut);

                if (index == 0)
                {
                    builder.BeginFigure(into);
                }
                else
                {
                    builder.AddLine(into);
                }

                builder.AddQuadraticBezier(corner, outOf);
            }

            builder.EndFigure(canvasGeometry::CanvasFigureLoop::Closed);

            return CompositionPath{ canvasGeometry::CanvasGeometry::CreatePath(builder) };
        }

        // Where a name of this size goes on one pad, as the top left corner of its text.
        float2 NamePosition(
            _In_ PadNoteNames placement,
            _In_ PadCell const& cell,
            _In_ PadGridLayout const& layout,
            _In_ double textWidth,
            _In_ double textHeight) noexcept
        {
            auto const halfWidth = layout.Hex
                ? layout.PadWidth * HexNameBoxWidth * 0.5
                : layout.PadWidth * (0.5 - NoteNameInset);

            auto const halfHeight = layout.Hex
                ? layout.PadHeight * HexNameBoxHeight * 0.5
                : layout.PadHeight * (0.5 - NoteNameInset);

            auto const left = cell.CenterX - halfWidth;
            auto const right = cell.CenterX + halfWidth;
            auto const top = cell.CenterY - halfHeight;
            auto const bottom = cell.CenterY + halfHeight;

            auto x = cell.CenterX - textWidth * 0.5;
            auto y = cell.CenterY - textHeight * 0.5;

            switch (placement)
            {
            case PadNoteNames::Top:
                y = top;
                break;

            case PadNoteNames::Bottom:
                y = bottom - textHeight;
                break;

            case PadNoteNames::TopLeft:
                x = left;
                y = top;
                break;

            case PadNoteNames::TopRight:
                x = right - textWidth;
                y = top;
                break;

            case PadNoteNames::BottomLeft:
                x = left;
                y = bottom - textHeight;
                break;

            case PadNoteNames::BottomRight:
                x = right - textWidth;
                y = bottom - textHeight;
                break;

            default:
                break;
            }

            return float2{ static_cast<float>(x), static_cast<float>(y) };
        }
    }

    _Use_decl_annotations_
    PadGridSpec const& SurfaceRenderer::PadGridAt(size_t itemIndex) const noexcept
    {
        static PadGridSpec const none{};

        return itemIndex < m_padGrids.size() ? m_padGrids[itemIndex] : none;
    }

    _Use_decl_annotations_
    PadGridLayout const& SurfaceRenderer::PadLayoutAt(size_t itemIndex) const noexcept
    {
        static PadGridLayout const none{};

        return itemIndex < m_visuals.size() ? m_visuals[itemIndex].PadLayout : none;
    }

    _Use_decl_annotations_
    ThemeColor SurfaceRenderer::PadBackdrop(Control const& control, Theme const& theme) const noexcept
    {
        // The same plate LayoutVisual paints, including what the control's own style says about
        // it, laid over the deck.
        auto const colors = ResolveControlColors(control, theme);

        auto plate = colors.Plate;

        if (control.Style == ControlStyleOverride::Outline || control.Style == ControlStyleOverride::Bare)
        {
            plate.A = 0;
        }
        else if (control.Style == ControlStyleOverride::Solid)
        {
            plate = colors.Pipe;
            plate.A = 170;
        }

        return BlendOver(m_deck, plate, 1.0);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutPads(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        Theme const& theme,
        float width,
        float height)
    {
        auto const hex = IsHexPadGrid(control.Kind);
        auto const& spec = control.Pads;

        visual.PadLayout = LayOutPadGrid(spec, hex, width, height);

        auto const& layout = visual.PadLayout;
        auto const count = layout.Cells.size();

        visual.PadShapes.assign(count, nullptr);
        visual.PadRestFills.assign(count, nullptr);
        visual.PadRestRims.assign(count, nullptr);
        visual.PadLitFills.assign(count, nullptr);
        visual.PadHeld.assign(count, 0);

        if (count == 0)
        {
            return;
        }

        auto const pads = ResolvePadColors(control, theme, PadBackdrop(control, theme));

        visual.PadLitRim = BrushFor(compositor, pads.LitRim).as<CompositionBrush>();

        auto const padWidth = static_cast<float>(layout.PadWidth);
        auto const padHeight = static_cast<float>(layout.PadHeight);

        // Every hexagon on one control is the same size, so they all share one path.
        CompositionPath hexPath{ nullptr };

        if (hex)
        {
            try
            {
                hexPath = HexagonPath(padWidth, padHeight, padHeight * 0.5f * HexCornerFraction);
            }
            catch (...)
            {
                // With no device to build a path on, a hexagon is drawn as a rounded box rather
                // than not at all. The pads still play, and still sit where a finger lands.
                hexPath = nullptr;
            }
        }

        auto const corner = std::min(static_cast<float>(theme.CornerRadius), padWidth * PadCornerFraction);

        // A shape at the origin, moved into place by the shape, so a gradient on it maps to the
        // pad rather than to the whole grid.
        auto const padShape = [&](float left, float top)
            {
                CompositionSpriteShape shape{ nullptr };

                if (hexPath != nullptr)
                {
                    shape = compositor.CreateSpriteShape(compositor.CreatePathGeometry(hexPath));
                }
                else
                {
                    auto geometry = compositor.CreateRoundedRectangleGeometry();

                    geometry.Size(float2{ padWidth, padHeight });
                    geometry.CornerRadius(float2{ corner, corner });

                    shape = compositor.CreateSpriteShape(geometry);
                }

                shape.Offset(float2{ left, top });

                return shape;
            };

        auto const falloff = PlateFalloff(colors);

        std::array<CompositionBrush, 3> restFaces{ nullptr, nullptr, nullptr };
        std::array<CompositionBrush, 3> litFaces{ nullptr, nullptr, nullptr };
        std::array<CompositionBrush, 3> rims{ nullptr, nullptr, nullptr };

        for (size_t role = 0; role < restFaces.size(); ++role)
        {
            restFaces[role] = PadFaceBrush(compositor, pads.RestFill[role], colors.Sheen, colors.PlateShade, falloff);
            litFaces[role] = PadFaceBrush(compositor, pads.LitFill[role], colors.Sheen, colors.PlateShade, falloff);

            if (pads.RestRim[role].A != 0)
            {
                rims[role] = BrushFor(compositor, pads.RestRim[role]).as<CompositionBrush>();
            }
        }

        auto const deadFill = BrushFor(compositor, pads.DeadFill).as<CompositionBrush>();

        // The light line inside the top edge, on a pad that has a flat top for it to run along.
        auto const lipped = hexPath == nullptr &&
            colors.PlateHighlight.A > 0 &&
            padWidth > corner * 2.0f + 2.0f;

        auto const lip = lipped ? BrushFor(compositor, colors.PlateHighlight).as<CompositionBrush>() : nullptr;

        // What the shadow is cut from: every pad that plays, in white.
        auto const casts = theme.PlateElevation > 0;

        ShapeVisual shadowSource{ nullptr };
        CompositionColorBrush shadowInk{ nullptr };

        if (casts)
        {
            shadowSource = compositor.CreateShapeVisual();
            shadowSource.Size(float2{ width, height });
            shadowInk = compositor.CreateColorBrush(winrt::Windows::UI::Colors::White());
        }

        for (size_t index = 0; index < count; ++index)
        {
            auto const& cell = layout.Cells[index];

            auto const left = static_cast<float>(cell.CenterX) - padWidth * 0.5f;
            auto const top = static_cast<float>(cell.CenterY) - padHeight * 0.5f;

            auto shape = padShape(left, top);

            CompositionBrush fill{ nullptr };
            CompositionBrush rim{ nullptr };
            CompositionBrush lit{ nullptr };

            // A pad off the end of the note range is there, so the grid keeps its shape, and
            // plainly dead: flat, no rim, no name, no shadow, and it never lights.
            auto const plays = cell.Note >= 0;

            if (!plays)
            {
                fill = deadFill;
            }
            else
            {
                auto const role = static_cast<size_t>(RoleOfNote(spec, cell.Note));

                fill = restFaces[role];
                rim = rims[role];
                lit = litFaces[role];
            }

            shape.FillBrush(fill);
            shape.StrokeBrush(rim);
            shape.StrokeThickness(PadRimThickness);

            visual.PadShapes[index] = shape;
            visual.PadRestFills[index] = fill;
            visual.PadRestRims[index] = rim;
            visual.PadLitFills[index] = lit;

            visual.ValueShape.Shapes().Append(shape);

            if (plays && lipped)
            {
                auto lipGeometry = compositor.CreateRoundedRectangleGeometry();
                lipGeometry.Size(float2{ padWidth - corner * 2.0f, 1.0f });

                auto lipShape = compositor.CreateSpriteShape(lipGeometry);
                lipShape.Offset(float2{ left + corner, top + 1.0f });
                lipShape.FillBrush(lip);

                visual.ValueShape.Shapes().Append(lipShape);
            }

            if (plays && casts)
            {
                auto caster = padShape(left, top);
                caster.FillBrush(shadowInk);

                shadowSource.Shapes().Append(caster);
            }
        }

        if (casts)
        {
            // One shadow for the whole grid, shaped like the pads, so a grid of a hundred costs
            // what one control's elevation costs.
            auto surface = compositor.CreateVisualSurface();
            surface.SourceVisual(shadowSource);
            surface.SourceSize(float2{ width, height });

            auto const spread = std::min(
                static_cast<float>(std::clamp(theme.ShadowSpread, 0, 64)),
                padWidth * PadShadowReach);

            auto shadowColor = theme.ShadowColor;
            shadowColor.A = 255;

            auto shadow = compositor.CreateDropShadow();

            shadow.BlurRadius(spread);
            shadow.Offset(float3{ 0.0f, spread / 3.0f, 0.0f });
            shadow.Color(ToWindowsColor(shadowColor));
            shadow.Opacity(static_cast<float>(std::clamp(theme.PlateElevation, 0, 100)) / 100.0f);
            shadow.Mask(compositor.CreateSurfaceBrush(surface));

            visual.PadShadow = compositor.CreateSpriteVisual();
            visual.PadShadow.Size(float2{ width, height });
            visual.PadShadow.Shadow(shadow);
            visual.PadShadowSource = shadowSource;

            visual.Root.Children().InsertBelow(visual.PadShadow, visual.ValueShape);
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetPadHeld(size_t itemIndex, int32_t cell, bool held) noexcept
    {
        if (itemIndex >= m_visuals.size() || cell < 0)
        {
            return;
        }

        auto& visual = m_visuals[itemIndex];
        auto const index = static_cast<size_t>(cell);

        if (index >= visual.PadShapes.size() || index >= visual.PadHeld.size())
        {
            return;
        }

        auto& fingers = visual.PadHeld[index];
        auto const wasLit = fingers > 0;

        if (held)
        {
            if (fingers < std::numeric_limits<uint8_t>::max())
            {
                fingers++;
            }
        }
        else if (fingers > 0)
        {
            fingers--;
        }

        auto const lit = fingers > 0;

        if (lit == wasLit)
        {
            return;
        }

        try
        {
            auto const& shape = visual.PadShapes[index];

            if (shape == nullptr || visual.PadLitFills[index] == nullptr)
            {
                return;
            }

            shape.FillBrush(lit ? visual.PadLitFills[index] : visual.PadRestFills[index]);
            shape.StrokeBrush(lit ? visual.PadLitRim : visual.PadRestRims[index]);

            if (itemIndex < m_padNames.size())
            {
                auto const& names = m_padNames[itemIndex];

                if (index < names.Texts.size() && names.Texts[index] != nullptr)
                {
                    names.Texts[index].Foreground(lit ? names.LitInks[index] : names.RestInks[index]);
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutPadNames(size_t itemIndex, Control const& control, Theme const& theme)
    {
        if (itemIndex >= m_padNames.size() || itemIndex >= m_visuals.size())
        {
            return;
        }

        auto& names = m_padNames[itemIndex];

        if (names.Host != nullptr)
        {
            uint32_t index{ 0 };

            if (m_host != nullptr && m_host.Children().IndexOf(names.Host, index))
            {
                m_host.Children().RemoveAt(index);
            }
        }

        names = PadNameTexts{};

        if (control.Kind == ControlKind::Switch)
        {
            LayoutSwitchLabels(itemIndex, control);
            return;
        }

        if (!IsPadGrid(control.Kind) || control.Pads.NoteNames == PadNoteNames::Hidden || m_host == nullptr)
        {
            return;
        }

        try
        {
            auto const& spec = control.Pads;
            auto const& visual = m_visuals[itemIndex];
            auto const& layout = visual.PadLayout;

            if (layout.Cells.empty())
            {
                return;
            }

            auto const colors = ResolvePadColors(control, theme, PadBackdrop(control, theme));

            // A key written with flats names its notes with flats: B flat in F major, not A sharp.
            auto const flats = spec.KeyRoot != NoKey && KeyUsesFlats(spec.KeyRoot, spec.Scale);

            auto const size = spec.NoteNameSize > 0.0
                ? spec.NoteNameSize
                : std::clamp(layout.PadWidth * NoteNameFraction, MinimumNoteNameSize, MaximumAutomaticNoteNameSize);

            controls::Canvas host{};

            host.IsHitTestVisible(false);
            host.Width(std::max(control.Width, 4.0));
            host.Height(std::max(control.Height, 4.0));

            // The control is one button to a screen reader. A name on every pad read out as well
            // would be sixty words of noise before it got to the next control.
            xaml::Automation::AutomationProperties::SetAccessibilityView(
                host, xaml::Automation::Peers::AccessibilityView::Raw);

            std::array<media::Brush, 3> restInks{ nullptr, nullptr, nullptr };
            std::array<media::Brush, 3> litInks{ nullptr, nullptr, nullptr };

            for (size_t role = 0; role < restInks.size(); ++role)
            {
                restInks[role] = media::SolidColorBrush{ ToWindowsColor(colors.RestInk[role]) };
                litInks[role] = media::SolidColorBrush{ ToWindowsColor(colors.LitInk[role]) };
            }

            auto const unbounded = winrt::Windows::Foundation::Size{
                std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity() };

            names.Texts.assign(layout.Cells.size(), nullptr);
            names.RestInks.assign(layout.Cells.size(), nullptr);
            names.LitInks.assign(layout.Cells.size(), nullptr);

            for (size_t index = 0; index < layout.Cells.size(); ++index)
            {
                auto const& cell = layout.Cells[index];

                if (cell.Note < 0)
                {
                    continue;
                }

                auto const role = static_cast<size_t>(RoleOfNote(spec, cell.Note));

                controls::TextBlock text{};

                text.Text(winrt::hstring{ PadNoteName(cell.Note, flats) });
                text.FontSize(size);
                text.FontFamily(media::FontFamily{ L"Segoe UI Variable Text" });
                text.FontWeight(winrt::Windows::UI::Text::FontWeight{ 600 });
                text.IsHitTestVisible(false);

                // A pad that was already lit when the page was laid out again keeps its lit ink.
                auto const lit = index < visual.PadHeld.size() && visual.PadHeld[index] > 0;

                text.Foreground(lit ? litInks[role] : restInks[role]);

                text.Measure(unbounded);

                auto const measured = text.DesiredSize();
                auto const at = NamePosition(spec.NoteNames, cell, layout, measured.Width, measured.Height);

                controls::Canvas::SetLeft(text, at.x);
                controls::Canvas::SetTop(text, at.y);

                host.Children().Append(text);

                names.Texts[index] = text;
                names.RestInks[index] = restInks[role];
                names.LitInks[index] = litInks[role];
            }

            controls::Canvas::SetLeft(host, control.X);
            controls::Canvas::SetTop(host, control.Y);

            m_host.Children().Append(host);
            names.Host = host;
        }
        catch (...)
        {
            names = PadNameTexts{};
        }
    }
}
