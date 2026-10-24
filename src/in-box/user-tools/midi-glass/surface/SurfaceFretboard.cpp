// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The fretboard: a window onto a neck, with its frets, strings, scale dots, the strum area and
// the strip that shows where on the neck the window is.
//
// Where everything goes is worked out in the document layer (Fretboard), so what is drawn here
// and what a finger lands on can never disagree. This file only paints it, in the theme's
// colors, so a fretboard on a dark theme looks like it belongs there.

#include "pch.h"
#include "SurfaceRenderer.h"
#include "Fretboard.h"
#include "FretboardChanges.h"
#include "PadGrid.h"

#include <cmath>
#include <limits>

using namespace winrt;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Windows::Foundation::Numerics;

namespace glass
{
    namespace
    {
        // Enough dots for every finger on two hands.
        constexpr size_t FingerDotCount = 10;

        constexpr float FretWireWidth = 2.0f;
        constexpr float NutWidth = 5.0f;
        constexpr float CapoWidth = 7.0f;

        // How dark the neck is under the frets on a theme with no well of its own, and how far an
        // out-of-key fret is dimmed.
        constexpr double NeckShade = 0.35;
        constexpr uint8_t DimAlpha = 120;

        winrt::Windows::UI::Color ToWindowsColor(_In_ ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        ThemeColor WithAlpha(_In_ ThemeColor color, _In_ uint8_t alpha) noexcept
        {
            color.A = alpha;
            return color;
        }

        // Lower strings are drawn thicker, the way they are on a real neck.
        float StringThickness(_In_ int32_t openNote, _In_ double pitch) noexcept
        {
            auto const thickness = std::clamp(1.0 + (72.0 - static_cast<double>(openNote)) / 14.0, 1.0, 4.5);

            return static_cast<float>(std::min(thickness, std::max(pitch * 0.2, 1.0)));
        }

        CompositionSpriteShape Box(
            _In_ Compositor const& compositor,
            _In_ float left,
            _In_ float top,
            _In_ float width,
            _In_ float height,
            _In_ float corner,
            _In_ CompositionBrush const& fill)
        {
            auto geometry = compositor.CreateRoundedRectangleGeometry();

            geometry.Size(float2{ std::max(width, 0.0f), std::max(height, 0.0f) });
            geometry.CornerRadius(float2{ corner, corner });

            auto shape = compositor.CreateSpriteShape(geometry);

            shape.Offset(float2{ left, top });
            shape.FillBrush(fill);

            return shape;
        }

        CompositionSpriteShape Dot(
            _In_ Compositor const& compositor,
            _In_ float centerX,
            _In_ float centerY,
            _In_ float radius,
            _In_ CompositionBrush const& fill)
        {
            auto geometry = compositor.CreateEllipseGeometry();

            geometry.Center(float2{ radius, radius });
            geometry.Radius(float2{ radius, radius });

            auto shape = compositor.CreateSpriteShape(geometry);

            shape.Offset(float2{ centerX - radius, centerY - radius });
            shape.FillBrush(fill);

            return shape;
        }

        // The left edge and the width of one fret's space between its wires, or of the open
        // column for fret 0.
        std::pair<float, float> FretSpan(_In_ FretboardLayout const& layout, _In_ int32_t fret) noexcept
        {
            if (fret <= 0 || layout.WireX.size() < 2)
            {
                return { static_cast<float>(layout.OpenLeft), static_cast<float>(layout.OpenRight - layout.OpenLeft) };
            }

            auto const cell = static_cast<size_t>(std::clamp(fret, layout.FirstCell, layout.LastCell) - layout.FirstCell);
            auto const a = layout.WireX[cell];
            auto const b = layout.WireX[cell + 1];

            return { static_cast<float>(std::min(a, b)), static_cast<float>(std::abs(b - a)) };
        }

        float DotRadius(_In_ FretboardLayout const& layout, _In_ int32_t fret) noexcept
        {
            auto const [left, width] = FretSpan(layout, fret);

            return static_cast<float>(std::max(2.0, std::min(layout.StringPitch * 0.3, static_cast<double>(width) * 0.32)));
        }
    }

    _Use_decl_annotations_
    FretboardSpec SurfaceRenderer::FretSpecFor(Control const& control) const
    {
        return NormalizedFretboard(WithFretState(control.Fretboard, StateOf(m_gridStates, control.Id)));
    }

    _Use_decl_annotations_
    FretboardSpec const& SurfaceRenderer::FretSpecAt(size_t itemIndex) const noexcept
    {
        static FretboardSpec const none{};

        return itemIndex < m_visuals.size() ? m_visuals[itemIndex].FretSpec : none;
    }

    _Use_decl_annotations_
    FretboardLayout const& SurfaceRenderer::FretLayoutAt(size_t itemIndex) const noexcept
    {
        static FretboardLayout const none{};

        return itemIndex < m_visuals.size() ? m_visuals[itemIndex].FretLayout : none;
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutFretboard(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        Theme const& theme,
        float width,
        float height)
    {
        visual.FretSpec = FretSpecFor(control);
        visual.FretLayout = LayOutFretboard(visual.FretSpec, width, height);
        visual.StringShapes.clear();
        visual.StringRestBrushes.clear();
        visual.FingerShapes.clear();

        auto const& spec = visual.FretSpec;
        auto const& layout = visual.FretLayout;
        auto const shapes = visual.ValueShape.Shapes();

        auto const backdrop = PadBackdrop(control, theme);
        auto const pads = ResolvePadColors(control, theme, backdrop);
        auto const ink = ReadableInk(backdrop);

        auto const neckColor = colors.Well.A != 0
            ? BlendOver(backdrop, colors.Well, 1.0)
            : BlendOver(backdrop, ThemeColor{ 0, 0, 0, 255 }, NeckShade);

        auto const neckInk = ReadableInk(neckColor);
        auto const wireColor = WithAlpha(neckInk, 140);
        auto const corner = static_cast<float>(std::min(static_cast<double>(theme.CornerRadius), 8.0));

        auto const neckTop = static_cast<float>(layout.StringsTop);
        auto const neckHeight = static_cast<float>(layout.StringsBottom - layout.StringsTop);

        auto const neckLeft = static_cast<float>(std::min(layout.OpenLeft, layout.NeckLeft));
        auto const neckRight = static_cast<float>(std::max(layout.OpenRight, layout.NeckRight));

        // The neck, and the open column darker at its end, where the head would be.
        shapes.Append(Box(compositor, neckLeft, neckTop, neckRight - neckLeft, neckHeight, corner,
            BrushFor(compositor, neckColor).as<CompositionBrush>()));

        shapes.Append(Box(compositor, static_cast<float>(layout.OpenLeft), neckTop,
            static_cast<float>(layout.OpenRight - layout.OpenLeft), neckHeight, corner,
            BrushFor(compositor, WithAlpha(ThemeColor{ 0, 0, 0, 255 }, 60)).as<CompositionBrush>()));

        auto const hasKey = HasFretKey(spec);
        auto const dims = hasKey && spec.OutOfKey != OutOfKeyFrets::Play;
        auto const dimBrush = BrushFor(compositor, WithAlpha(ThemeColor{ 0, 0, 0, 255 }, DimAlpha)).as<CompositionBrush>();

        // Out-of-key frets, dimmed one string at a time, and frets behind the capo, which play
        // nothing, dimmed across every string.
        for (auto fret = layout.FirstCell; fret <= layout.LastCell; ++fret)
        {
            auto const [left, span] = FretSpan(layout, fret);

            if (spec.Capo > 0 && fret < spec.Capo)
            {
                shapes.Append(Box(compositor, left, neckTop, span, neckHeight, 0.0f, dimBrush));
                continue;
            }

            if (!dims)
            {
                continue;
            }

            for (size_t string = 0; string < layout.StringY.size(); ++string)
            {
                auto const note = FretNote(spec, static_cast<int32_t>(string), fret);

                if (note >= 0 && IsInScale(spec.Scale, spec.KeyRoot, note))
                {
                    continue;
                }

                auto const top = static_cast<float>(layout.StringY[string] - layout.StringPitch * 0.5);

                shapes.Append(Box(compositor, left, top, span, static_cast<float>(layout.StringPitch), 0.0f, dimBrush));
            }
        }

        // Inlay dots, where a real neck has them, so a player can find the fifth and the twelfth
        // fret without counting.
        auto const inlayBrush = BrushFor(compositor, WithAlpha(neckInk, 50)).as<CompositionBrush>();
        auto const middle = static_cast<float>((layout.StringsTop + layout.StringsBottom) / 2.0);

        for (auto fret = layout.FirstCell; fret <= layout.LastCell; ++fret)
        {
            auto const dots = InlayDots(fret);
            auto const x = static_cast<float>(FretCenterX(layout, fret));
            auto const radius = std::max(2.0f, DotRadius(layout, fret) * 0.55f);

            if (dots == 1)
            {
                shapes.Append(Dot(compositor, x, middle, radius, inlayBrush));
            }
            else if (dots == 2)
            {
                shapes.Append(Dot(compositor, x, neckTop + neckHeight * 0.3f, radius, inlayBrush));
                shapes.Append(Dot(compositor, x, neckTop + neckHeight * 0.7f, radius, inlayBrush));
            }
        }

        // The wires, and the nut when the frets showing start at the nut.
        auto const wireBrush = BrushFor(compositor, wireColor).as<CompositionBrush>();

        for (size_t wire = 0; wire < layout.WireX.size(); ++wire)
        {
            auto const isNut = wire == 0 && layout.FirstCell == 1;
            auto const thickness = isNut ? NutWidth : FretWireWidth;

            shapes.Append(Box(compositor, static_cast<float>(layout.WireX[wire]) - thickness * 0.5f, neckTop,
                thickness, neckHeight, 0.0f, isNut ? BrushFor(compositor, neckInk).as<CompositionBrush>() : wireBrush));
        }

        // The capo, a bar across every string just behind its fret.
        if (spec.Capo > 0 && spec.Capo >= layout.FirstCell && spec.Capo <= layout.LastCell)
        {
            auto const [left, span] = FretSpan(layout, spec.Capo);
            auto const x = layout.Mirrored ? left + span * 0.8f : left + span * 0.2f;

            shapes.Append(Box(compositor, x - CapoWidth * 0.5f, neckTop + 2.0f, CapoWidth, neckHeight - 4.0f, CapoWidth * 0.5f,
                BrushFor(compositor, WithAlpha(ink, 210)).as<CompositionBrush>()));
        }

        // The scale dots, the root in its own color, on every fret that shows. The open column
        // prints each string's name instead, which a dot would cover.
        if (hasKey)
        {
            std::array<CompositionBrush, 3> dotBrushes{ nullptr, nullptr, nullptr };

            for (size_t role = 0; role < dotBrushes.size(); ++role)
            {
                dotBrushes[role] = BrushFor(compositor, pads.RestFill[role]).as<CompositionBrush>();
            }

            for (size_t string = 0; string < layout.StringY.size(); ++string)
            {
                for (auto fret = layout.FirstCell; fret <= layout.LastCell; ++fret)
                {
                    auto const note = FretNote(spec, static_cast<int32_t>(string), fret);

                    if (note < 0 || !IsInScale(spec.Scale, spec.KeyRoot, note))
                    {
                        continue;
                    }

                    auto const role = (note % 12) == spec.KeyRoot ? 2u : 1u;
                    auto const radius = DotRadius(layout, fret) * (role == 2 ? 1.0f : 0.8f);

                    shapes.Append(Dot(compositor, static_cast<float>(FretCenterX(layout, fret)),
                        static_cast<float>(layout.StringY[string]), radius, dotBrushes[role]));
                }
            }
        }

        // The strings, across the neck and on through the strum area.
        auto const restString = BrushFor(compositor, WithAlpha(neckInk, 200)).as<CompositionBrush>();

        visual.StringLitBrush = BrushFor(compositor, pads.LitFill[1]).as<CompositionBrush>();

        auto const stringLeft = layout.HasStrum ? std::min(neckLeft, static_cast<float>(layout.StrumLeft)) : neckLeft;
        auto const stringRight = layout.HasStrum ? std::max(neckRight, static_cast<float>(layout.StrumRight)) : neckRight;

        // The strum area under the strings, ridged so it reads as somewhere to sweep.
        if (layout.HasStrum)
        {
            auto const strumColor = BlendOver(backdrop, ink, 0.06);

            shapes.Append(Box(compositor, static_cast<float>(layout.StrumLeft), neckTop,
                static_cast<float>(layout.StrumRight - layout.StrumLeft), neckHeight, corner,
                BrushFor(compositor, strumColor).as<CompositionBrush>()));

            auto const ridgeBrush = BrushFor(compositor, WithAlpha(ink, 22)).as<CompositionBrush>();

            for (auto x = static_cast<float>(layout.StrumLeft) + 6.0f; x < static_cast<float>(layout.StrumRight) - 3.0f; x += 12.0f)
            {
                shapes.Append(Box(compositor, x, neckTop + 4.0f, 3.0f, std::max(neckHeight - 8.0f, 0.0f), 1.0f, ridgeBrush));
            }
        }

        for (size_t string = 0; string < layout.StringY.size(); ++string)
        {
            auto const thickness = StringThickness(spec.Strings[string].Note, layout.StringPitch);
            auto const y = static_cast<float>(layout.StringY[string]) - thickness * 0.5f;

            // A short string starts at its own fret, not at the nut.
            auto left = stringLeft;
            auto right = stringRight;
            auto const start = spec.Strings[string].StartFret;

            if (start > 0 && start >= layout.FirstCell && start <= layout.LastCell)
            {
                auto const at = static_cast<float>(layout.WireX[static_cast<size_t>(start - layout.FirstCell + 1)]);

                (layout.Mirrored ? right : left) = at;
            }

            auto shape = Box(compositor, left, y, right - left, thickness, thickness * 0.5f, restString);

            shapes.Append(shape);
            visual.StringShapes.push_back(shape);
            visual.StringRestBrushes.push_back(restString);
        }

        // The neck strip: the whole neck in miniature, with the part that shows marked.
        if (layout.HasStrip)
        {
            auto const stripTop = static_cast<float>(layout.StripTop);
            auto const stripHeight = static_cast<float>(layout.StripBottom - layout.StripTop);

            shapes.Append(Box(compositor, neckLeft, stripTop, neckRight - neckLeft, stripHeight, stripHeight * 0.3f,
                BrushFor(compositor, BlendOver(backdrop, ThemeColor{ 0, 0, 0, 255 }, 0.25)).as<CompositionBrush>()));

            auto const pipBrush = BrushFor(compositor, WithAlpha(ink, 90)).as<CompositionBrush>();

            for (auto fret = 1; fret <= spec.Frets; ++fret)
            {
                if (InlayDots(fret) == 0)
                {
                    continue;
                }

                auto const x = static_cast<float>(StripX(spec, layout, fret - 0.5));

                shapes.Append(Dot(compositor, x, stripTop + stripHeight * 0.5f, std::max(1.5f, stripHeight * 0.12f), pipBrush));
            }

            auto const from = static_cast<float>(StripX(spec, layout, std::max(spec.FirstFret, 1) - 1));
            auto const to = static_cast<float>(StripX(spec, layout, spec.LastFret));

            auto window = Box(compositor, std::min(from, to), stripTop + 1.0f, std::abs(to - from), stripHeight - 2.0f,
                stripHeight * 0.3f, BrushFor(compositor, WithAlpha(colors.Pipe, 90)).as<CompositionBrush>());

            window.StrokeBrush(BrushFor(compositor, colors.Pipe).as<CompositionBrush>());
            window.StrokeThickness(1.0f);

            shapes.Append(window);
        }

        // A dot for each finger, hidden until a finger is down.
        auto const fingerBrush = BrushFor(compositor, pads.LitFill[1]).as<CompositionBrush>();
        auto const fingerRadius = static_cast<float>(std::max(4.0, layout.StringPitch * 0.36));

        for (size_t index = 0; index < FingerDotCount; ++index)
        {
            auto finger = Dot(compositor, 0.0f, 0.0f, fingerRadius, fingerBrush);

            finger.StrokeBrush(BrushFor(compositor, pads.LitRim).as<CompositionBrush>());
            finger.StrokeThickness(1.5f);
            finger.Scale(float2{ 0.0f, 0.0f });

            shapes.Append(finger);
            visual.FingerShapes.push_back(finger);
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetFretFingers(size_t itemIndex, std::span<FretFingerMark const> marks) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        try
        {
            auto& visual = m_visuals[itemIndex];
            auto const& layout = visual.FretLayout;

            for (size_t index = 0; index < visual.FingerShapes.size(); ++index)
            {
                auto const& shape = visual.FingerShapes[index];

                if (index >= marks.size() ||
                    marks[index].String < 0 ||
                    static_cast<size_t>(marks[index].String) >= layout.StringY.size())
                {
                    shape.Scale(float2{ 0.0f, 0.0f });
                    continue;
                }

                auto const& mark = marks[index];
                auto const geometry = shape.Geometry().as<CompositionEllipseGeometry>();
                auto const radius = geometry.Radius().x;

                // A finger bending the string carries the string with it, up to the next one.
                auto const across = std::clamp(mark.Across, -layout.StringPitch, layout.StringPitch);
                auto const x = static_cast<float>(FretCenterX(layout, mark.Fret));
                auto const y = static_cast<float>(layout.StringY[static_cast<size_t>(mark.String)] + across);

                shape.Offset(float2{ x - radius, y - radius });
                shape.Scale(float2{ 1.0f, 1.0f });
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetStringsSounding(size_t itemIndex, uint32_t sounding) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        try
        {
            auto& visual = m_visuals[itemIndex];

            for (size_t string = 0; string < visual.StringShapes.size() && string < visual.StringRestBrushes.size(); ++string)
            {
                auto const lit = (sounding & (1u << string)) != 0;

                visual.StringShapes[string].FillBrush(lit ? visual.StringLitBrush : visual.StringRestBrushes[string]);

                if (itemIndex < m_padNames.size())
                {
                    auto const& names = m_padNames[itemIndex];

                    if (string < names.Texts.size() && names.Texts[string] != nullptr)
                    {
                        names.Texts[string].Foreground(lit ? names.LitInks[string] : names.RestInks[string]);
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutFretNames(size_t itemIndex, Control const& control, Theme const& theme)
    {
        if (itemIndex >= m_padNames.size() || itemIndex >= m_visuals.size() || m_host == nullptr)
        {
            return;
        }

        auto& names = m_padNames[itemIndex];

        try
        {
            auto const& visual = m_visuals[itemIndex];
            auto const& spec = visual.FretSpec;
            auto const& layout = visual.FretLayout;

            if (layout.StringY.empty() || layout.OpenRight - layout.OpenLeft < 12.0)
            {
                return;
            }

            auto const size = std::clamp(layout.StringPitch * 0.42, 7.0, 16.0);
            auto const flats = HasFretKey(spec) && KeyUsesFlats(spec.KeyRoot, spec.Scale);

            auto const backdrop = PadBackdrop(control, theme);
            auto const pads = ResolvePadColors(control, theme, backdrop);
            auto const restInk = media::SolidColorBrush{ ToWindowsColor(ReadableInk(BlendOver(backdrop, ThemeColor{ 0, 0, 0, 255 }, NeckShade))) };
            auto const litInk = media::SolidColorBrush{ ToWindowsColor(pads.LitFill[1]) };

            controls::Canvas host{};

            host.IsHitTestVisible(false);
            host.Width(std::max(control.Width, 4.0));
            host.Height(std::max(control.Height, 4.0));

            // The control is one button to a screen reader, and its strings are part of it.
            xaml::Automation::AutomationProperties::SetAccessibilityView(
                host, xaml::Automation::Peers::AccessibilityView::Raw);

            auto const unbounded = winrt::Windows::Foundation::Size{
                std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity() };

            names.Texts.assign(layout.StringY.size(), nullptr);
            names.RestInks.assign(layout.StringY.size(), nullptr);
            names.LitInks.assign(layout.StringY.size(), nullptr);

            for (size_t string = 0; string < layout.StringY.size(); ++string)
            {
                auto const note = FretNote(spec, static_cast<int32_t>(string), 0);

                if (note < 0)
                {
                    continue;
                }

                controls::TextBlock text{};

                text.Text(winrt::hstring{ PadNoteName(note, flats) });
                text.FontSize(size);
                text.FontFamily(media::FontFamily{ L"Segoe UI Variable Text" });
                text.FontWeight(winrt::Windows::UI::Text::FontWeight{ 600 });
                text.IsHitTestVisible(false);
                text.Foreground(restInk);
                text.Measure(unbounded);

                auto const measured = text.DesiredSize();

                controls::Canvas::SetLeft(text, (layout.OpenLeft + layout.OpenRight - measured.Width) / 2.0);
                controls::Canvas::SetTop(text, layout.StringY[string] - measured.Height - 1.0);

                host.Children().Append(text);

                names.Texts[string] = text;
                names.RestInks[string] = restInk;
                names.LitInks[string] = litInk;
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
