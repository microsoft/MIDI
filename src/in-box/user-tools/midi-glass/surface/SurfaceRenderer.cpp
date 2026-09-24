// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SurfaceRenderer.h"
#include "GlassControl.h"

using namespace winrt;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml::Hosting;

namespace glass
{
    namespace
    {
        // The value pipe sits this far inside the plate, so the rim stays a clean hairline.
        constexpr float PipeInset = 8.0f;

        constexpr float KnobArcThickness = 4.0f;

        // Leaves a gap at the bottom of a knob, the way every hardware knob has an end stop.
        constexpr float KnobSweepDegrees = 270.0f;
        constexpr float KnobTrimOffset = 0.625f;

        constexpr float BloomSpread = 6.0f;

        // Long enough to see across a room, short enough not to smear into the next hit.
        constexpr int64_t BloomDecayMilliseconds = 220;

        winrt::Windows::UI::Color ToColor(_In_ ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        uint32_t ColorKey(_In_ ThemeColor const& color) noexcept
        {
            return (static_cast<uint32_t>(color.A) << 24) |
                (static_cast<uint32_t>(color.R) << 16) |
                (static_cast<uint32_t>(color.G) << 8) |
                static_cast<uint32_t>(color.B);
        }

        bool IsRoundControl(_In_ ControlKind kind) noexcept
        {
            return kind == ControlKind::Knob || kind == ControlKind::Encoder;
        }

        bool IsTallControl(_In_ ControlKind kind, _In_ double width, _In_ double height) noexcept
        {
            if (kind == ControlKind::Fader || kind == ControlKind::Meter)
            {
                // A fader laid out wider than it is tall is a horizontal fader, which is a real
                // thing people build, so the shape follows the rectangle rather than the name.
                return height >= width;
            }

            return false;
        }

        bool DrawsAPipe(_In_ ControlKind kind) noexcept
        {
            switch (kind)
            {
            case ControlKind::Label:
            case ControlKind::Image:
                return false;

            default:
                return true;
            }
        }

        projected::SurfaceControlRole RoleFor(_In_ ControlKind kind) noexcept
        {
            switch (kind)
            {
            case ControlKind::Pad:
            case ControlKind::Button:
            case ControlKind::PageTab:
                return projected::SurfaceControlRole::Button;

            case ControlKind::Toggle:
                return projected::SurfaceControlRole::Toggle;

            case ControlKind::Label:
            case ControlKind::Readout:
            case ControlKind::Image:
            case ControlKind::Lamp:
            case ControlKind::Meter:
                return projected::SurfaceControlRole::Text;

            default:
                return projected::SurfaceControlRole::Slider;
            }
        }
    }

    _Use_decl_annotations_
    CompositionColorBrush SurfaceRenderer::BrushFor(
        Compositor const& compositor,
        ThemeColor const& color)
    {
        auto const key = ColorKey(color);
        auto const existing = m_brushes.find(key);

        if (existing != m_brushes.end())
        {
            return existing->second;
        }

        auto brush = compositor.CreateColorBrush(ToColor(color));

        m_brushes.emplace(key, brush);

        return brush;
    }

    _Use_decl_annotations_
    void SurfaceRenderer::Build(
        controls::Canvas const& host,
        LayoutDocument const& document,
        Theme const& theme,
        size_t pageIndex)
    {
        Teardown();

        m_host = host;
        m_deck = theme.Deck.Color;

        if (host == nullptr || pageIndex >= document.Pages.size())
        {
            return;
        }

        auto const compositor = ElementCompositionPreview::GetElementVisual(host).Compositor();

        // The engine counts every control in the document, page by page. The surface shows one
        // page, so it has to start counting from where that page begins.
        uint32_t controlIndex{ 0 };

        for (size_t i = 0; i < pageIndex; ++i)
        {
            controlIndex += static_cast<uint32_t>(document.Pages[i].Controls.size());
        }

        auto const& page = document.Pages[pageIndex];

        m_visuals.reserve(page.Controls.size());
        m_elements.reserve(page.Controls.size());
        m_controlIndexes.reserve(page.Controls.size());
        m_kinds.reserve(page.Controls.size());

        for (auto const& control : page.Controls)
        {
            BuildControl(compositor, control, theme, controlIndex);
            controlIndex++;
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::BuildControl(
        Compositor const& compositor,
        Control const& control,
        Theme const& theme,
        uint32_t controlIndex)
    {
        auto const width = static_cast<float>(std::max(control.Width, 4.0));
        auto const height = static_cast<float>(std::max(control.Height, 4.0));

        auto const colors = ResolveControlColors(control, theme);

        GlassControlElement element{};

        element.Width(width);
        element.Height(height);
        element.SurfaceName(winrt::hstring{ control.Label.empty() ? control.Id : control.Label });
        element.SurfaceRole(RoleFor(control.Kind));
        element.IsTabStop(RoleFor(control.Kind) != projected::SurfaceControlRole::Text);
        element.UseSystemFocusVisuals(true);

        // Nothing is drawn by XAML, so the element still has to be hit testable. A transparent
        // background is the usual way, and it is why this is a Control rather than a bare
        // FrameworkElement.
        element.Background(media::SolidColorBrush(winrt::Windows::UI::Colors::Transparent()));

        winrt::get_self<winrt::midiglass::implementation::GlassControl>(element)->ControlIndex(controlIndex);

        controls::Canvas::SetLeft(element, control.X);
        controls::Canvas::SetTop(element, control.Y);

        m_host.Children().Append(element);

        SurfaceVisual visual{};

        visual.Kind = control.Kind;
        visual.Width = width;
        visual.Height = height;

        visual.Root = compositor.CreateContainerVisual();
        visual.Root.Size(float2{ width, height });

        // The bloom sits behind the plate. Changing a visual's opacity invalidates no layout,
        // which is what makes a page of blinking controls free.
        visual.Bloom = compositor.CreateSpriteVisual();
        visual.Bloom.Brush(BrushFor(compositor, colors.Bloom));
        visual.Bloom.Offset(float3{ -BloomSpread, -BloomSpread, 0.0f });
        visual.Bloom.Size(float2{ width + BloomSpread * 2, height + BloomSpread * 2 });
        visual.Bloom.Opacity(0.0f);
        visual.Root.Children().InsertAtTop(visual.Bloom);

        visual.Shape = compositor.CreateShapeVisual();
        visual.Shape.Size(float2{ width, height });
        visual.Root.Children().InsertAtTop(visual.Shape);

        auto const corner = static_cast<float>(std::min(
            static_cast<double>(theme.CornerRadius), std::min(width, height) / 2.0));

        auto plateGeometry = compositor.CreateRoundedRectangleGeometry();
        plateGeometry.Size(float2{ width - 1.0f, height - 1.0f });
        plateGeometry.Offset(float2{ 0.5f, 0.5f });
        plateGeometry.CornerRadius(float2{ corner, corner });

        visual.PlateShape = compositor.CreateSpriteShape(plateGeometry);
        visual.PlateShape.FillBrush(BrushFor(compositor, colors.Plate));

        if (colors.Rim.A != 0)
        {
            visual.PlateShape.StrokeBrush(BrushFor(compositor, colors.Rim));
            visual.PlateShape.StrokeThickness(1.0f);
        }

        visual.Shape.Shapes().Append(visual.PlateShape);

        if (DrawsAPipe(control.Kind))
        {
            if (IsRoundControl(control.Kind))
            {
                auto const radius = std::min(width, height) * 0.5f - PipeInset;

                auto trackGeometry = compositor.CreateEllipseGeometry();
                trackGeometry.Radius(float2{ radius, radius });
                trackGeometry.Center(float2{ width * 0.5f, height * 0.5f });
                trackGeometry.TrimOffset(KnobTrimOffset);
                trackGeometry.TrimStart(0.0f);
                trackGeometry.TrimEnd(KnobSweepDegrees / 360.0f);

                auto trackShape = compositor.CreateSpriteShape(trackGeometry);
                trackShape.StrokeBrush(BrushFor(compositor, colors.Track));
                trackShape.StrokeThickness(KnobArcThickness);
                visual.Shape.Shapes().Append(trackShape);

                visual.ArcGeometry = compositor.CreateEllipseGeometry();
                visual.ArcGeometry.Radius(float2{ radius, radius });
                visual.ArcGeometry.Center(float2{ width * 0.5f, height * 0.5f });
                visual.ArcGeometry.TrimOffset(KnobTrimOffset);
                visual.ArcGeometry.TrimStart(0.0f);
                visual.ArcGeometry.TrimEnd(0.0f);

                visual.PipeShape = compositor.CreateSpriteShape(visual.ArcGeometry);
                visual.PipeShape.StrokeBrush(BrushFor(compositor, colors.Pipe));
                visual.PipeShape.StrokeThickness(KnobArcThickness);
                visual.Shape.Shapes().Append(visual.PipeShape);
            }
            else
            {
                auto const vertical = IsTallControl(control.Kind, control.Width, control.Height);

                auto const slot = std::max(6.0f, std::min(width, height) * 0.2f);

                auto const trackX = vertical ? (width * 0.5f - slot * 0.5f) : PipeInset;
                auto const trackY = vertical ? PipeInset : (height - PipeInset - slot);
                auto const trackW = vertical ? slot : (width - PipeInset * 2);
                auto const trackH = vertical ? (height - PipeInset * 2) : slot;

                auto trackGeometry = compositor.CreateRoundedRectangleGeometry();
                trackGeometry.Size(float2{ std::max(trackW, 1.0f), std::max(trackH, 1.0f) });
                trackGeometry.Offset(float2{ trackX, trackY });
                trackGeometry.CornerRadius(float2{ slot * 0.5f, slot * 0.5f });

                auto trackShape = compositor.CreateSpriteShape(trackGeometry);
                trackShape.FillBrush(BrushFor(compositor, colors.Track));
                visual.Shape.Shapes().Append(trackShape);

                visual.PipeGeometry = compositor.CreateRoundedRectangleGeometry();
                visual.PipeGeometry.CornerRadius(float2{ slot * 0.5f, slot * 0.5f });
                visual.PipeGeometry.Size(float2{ vertical ? trackW : 0.0f, vertical ? 0.0f : trackH });
                visual.PipeGeometry.Offset(float2{ trackX, vertical ? (trackY + trackH) : trackY });

                visual.PipeShape = compositor.CreateSpriteShape(visual.PipeGeometry);
                visual.PipeShape.FillBrush(BrushFor(compositor, colors.Pipe));
                visual.Shape.Shapes().Append(visual.PipeShape);

                visual.TrackOrigin = vertical ? trackY : trackX;
                visual.TrackLength = vertical ? trackH : trackW;
                visual.PipeThickness = vertical ? trackW : trackH;
                visual.PipeCrossOffset = vertical ? trackX : trackY;
            }
        }

        ElementCompositionPreview::SetElementChildVisual(element, visual.Root);

        m_visuals.push_back(std::move(visual));
        m_elements.push_back(element);
        m_controlIndexes.push_back(controlIndex);
        m_kinds.push_back(control.Kind);

        auto const itemIndex = m_visuals.size() - 1;

        SetValue(itemIndex, control.DefaultValue);
        winrt::get_self<winrt::midiglass::implementation::GlassControl>(element)
            ->SetValueDirect(control.DefaultValue);

        if (theme.Labels != LabelPlacement::None && !control.Label.empty())
        {
            controls::TextBlock label{};

            label.Text(winrt::hstring{ control.Label });
            label.FontSize(12);
            label.TextAlignment(xaml::TextAlignment::Center);
            label.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            label.Width(width);
            label.IsHitTestVisible(false);
            label.Foreground(media::SolidColorBrush(ToColor(colors.Label)));

            // The control beside it already carries the name, so a screen reader reading this as
            // well would say everything twice.
            xaml::Automation::AutomationProperties::SetAccessibilityView(
                label, xaml::Automation::Peers::AccessibilityView::Raw);

            controls::Canvas::SetLeft(label, control.X);
            controls::Canvas::SetTop(label, theme.Labels == LabelPlacement::Below
                ? control.Y + height + 2
                : control.Y + height - 18);

            m_host.Children().Append(label);
        }
    }

    void SurfaceRenderer::Teardown()
    {
        for (auto const& element : m_elements)
        {
            if (element != nullptr)
            {
                ElementCompositionPreview::SetElementChildVisual(element, nullptr);
            }
        }

        if (m_host != nullptr)
        {
            m_host.Children().Clear();
        }

        m_visuals.clear();
        m_elements.clear();
        m_controlIndexes.clear();
        m_kinds.clear();
        m_brushes.clear();
        m_host = nullptr;
    }

    _Use_decl_annotations_
    uint32_t SurfaceRenderer::ControlIndexOf(size_t itemIndex) const noexcept
    {
        return itemIndex < m_controlIndexes.size() ? m_controlIndexes[itemIndex] : 0u;
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::TryFindItem(uint32_t controlIndex, size_t& itemIndex) const noexcept
    {
        itemIndex = 0;

        for (size_t i = 0; i < m_controlIndexes.size(); ++i)
        {
            if (m_controlIndexes[i] == controlIndex)
            {
                itemIndex = i;
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    GlassControlElement SurfaceRenderer::ElementAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_elements.size() ? m_elements[itemIndex] : nullptr;
    }

    _Use_decl_annotations_
    ControlKind SurfaceRenderer::KindAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_kinds.size() ? m_kinds[itemIndex] : ControlKind::Knob;
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetValue(size_t itemIndex, double value) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto const& visual = m_visuals[itemIndex];
        auto const clamped = static_cast<float>(std::clamp(value, 0.0, 1.0));

        try
        {
            if (visual.ArcGeometry != nullptr)
            {
                visual.ArcGeometry.TrimEnd(clamped * (KnobSweepDegrees / 360.0f));
            }
            else if (visual.PipeGeometry != nullptr)
            {
                auto const vertical = IsTallControl(visual.Kind, visual.Width, visual.Height);
                auto const filled = visual.TrackLength * clamped;

                if (vertical)
                {
                    visual.PipeGeometry.Size(float2{ visual.PipeThickness, filled });
                    visual.PipeGeometry.Offset(float2{
                        visual.PipeCrossOffset,
                        visual.TrackOrigin + visual.TrackLength - filled });
                }
                else
                {
                    visual.PipeGeometry.Size(float2{ filled, visual.PipeThickness });
                    visual.PipeGeometry.Offset(float2{ visual.TrackOrigin, visual.PipeCrossOffset });
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::Bloom(size_t itemIndex) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto const& visual = m_visuals[itemIndex];

        if (visual.Bloom == nullptr)
        {
            return;
        }

        try
        {
            if (m_reducedMotion)
            {
                // A switch rather than a throb. It still says "this one just did something",
                // which is the whole job.
                visual.Bloom.Opacity(1.0f);
                return;
            }

            auto const compositor = visual.Bloom.Compositor();

            auto animation = compositor.CreateScalarKeyFrameAnimation();
            animation.InsertKeyFrame(0.0f, 1.0f);
            animation.InsertKeyFrame(1.0f, 0.0f);
            animation.Duration(std::chrono::milliseconds{ BloomDecayMilliseconds });

            visual.Bloom.StartAnimation(L"Opacity", animation);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::ClearBloom(size_t itemIndex) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto const& visual = m_visuals[itemIndex];

        if (visual.Bloom == nullptr)
        {
            return;
        }

        try
        {
            visual.Bloom.StopAnimation(L"Opacity");
            visual.Bloom.Opacity(0.0f);
        }
        catch (...)
        {
        }
    }
}
