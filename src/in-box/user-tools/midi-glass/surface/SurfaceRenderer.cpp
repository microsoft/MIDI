// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SurfaceRenderer.h"
#include "InputRules.h"
#include "LayoutStore.h"
#include "GlassControl.h"
#include "StringResources.h"

using namespace winrt;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml::Hosting;

namespace resources = ::midiglass::resources;

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

        // How far the light around a lit control carries. Two radii, because the plate glows
        // wider and softer than the thin bar that is carrying the value.
        constexpr float GlowBlurRadius = 16.0f;

        // A button's value strip: a bar along one edge rather than a slot through the middle.
        constexpr float StripThickness = 2.5f;
        constexpr float StripEdgeInset = 6.0f;

        // The cap on a fader. The comp draws it 30 x 16 on a 37.5 px fader: eight tenths of the
        // control across, and a little over half that again thick. All of it is proportional -
        // a fixed thickness turns into a thin strip the moment somebody draws a wider fader.
        constexpr float ThumbSpanFraction = 0.80f;
        constexpr float ThumbAspect = 16.0f / 30.0f;
        constexpr float ThumbCornerFraction = 4.0f / 16.0f;

        // The hairline of hue through the cap, against the cap's own size.
        constexpr float ThumbLineSpanFraction = 20.0f / 30.0f;
        constexpr float ThumbLineAspect = 2.0f / 16.0f;

        // A cap may not eat the travel it is supposed to ride.
        constexpr float MaximumThumbShareOfTravel = 0.34f;

        // Below this a cap is bigger than the control it sits on, so the fader draws its fill
        // alone rather than a cap with a sliver of slot behind it.
        constexpr float MinimumThumbSize = 34.0f;

        // Where the sheen has finished falling off, as a fraction of the plate's height.
        constexpr float SheenFalloff = 0.42f;

        // A switch that is on stays lit rather than decaying, but not at the full strength a
        // fresh hit gets, or a page of latched buttons is the brightest thing in the room.
        constexpr float SwitchOnGlowOpacity = 0.55f;

        constexpr float HatchSpacing = 9.0f;
        constexpr float HatchThickness = 2.0f;

        // How far down a control goes when the device it sends to is not here. Struck through
        // and dimmed, never hidden: a layout with a missing device still has to be editable.
        constexpr float UnavailableOpacity = 0.55f;

        // A ribbon's light when nobody is touching it. Still readable as a position, clearly
        // not the same thing as a finger being on the control.
        constexpr float RibbonRestOpacity = 0.45f;

        // How much of each lamp's slice is lamp rather than gap. Too low and the ring reads as a
        // dotted line rather than a row of lamps.
        constexpr float LampDutyCycle = 0.66f;

        // The pointer on a knob, against the dial's diameter. The comp draws it 2 px wide and
        // three tenths of the dial long, starting a tenth of the way in from the top.
        constexpr float PointerWidthFraction = 0.025f;
        constexpr float PointerLengthFraction = 0.30f;
        constexpr float PointerClearance = 2.0f;
        constexpr float CenterDotFraction = 0.045f;
        constexpr float DetentTickLength = 5.0f;

        // A knob's travel: three quarters of a turn, starting at seven o'clock. The pointer is
        // authored pointing straight up, which is the middle of that range.
        constexpr float KnobStartAngle = -135.0f;

        // How far across the control a fader's tick marks reach.
        constexpr float FaderTickSpan = 0.55f;

        // The halo around a value bar. Measured off the comp: at the bar's edge it is a little
        // under half the bar's own strength, and it has fallen to nothing about one slot width
        // out, on a gaussian curve.
        constexpr int32_t GlowRingCount = 12;
        constexpr float GlowReachFraction = 1.10f;
        constexpr double GlowPeakAlpha = 0.58;
        constexpr double GlowTightness = 5.0;

        // A ring fainter than this is a step of one in eight bits. Drawing it costs a shape and
        // changes nothing.
        constexpr double GlowMinimumStep = 0.004;

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
            return kind == ControlKind::Knob ||
                kind == ControlKind::Encoder ||
                kind == ControlKind::Joystick ||
                kind == ControlKind::Turntable;
        }

        // A control that draws its own thing inside the plate rather than a track and a bar.
        bool DrawsItsOwnValue(_In_ ControlKind kind) noexcept
        {
            switch (kind)
            {
            case ControlKind::XYPad:
            case ControlKind::Joystick:
            case ControlKind::Ribbon:
            case ControlKind::PianoKeyboard:
            case ControlKind::BeatClock:
            case ControlKind::TimeDisplay:
            case ControlKind::Lfo:
            case ControlKind::Turntable:
                return true;

            default:
                return false;
            }
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
            case ControlKind::Panel:
                return false;

            default:
                return !DrawsItsOwnValue(kind);
            }
        }

        // A control with travel: the value moves along a slot and a cap can ride it. Everything
        // else shows its value as a strip along an edge.
        bool DrawsATrack(_In_ ControlKind kind) noexcept
        {
            switch (kind)
            {
            case ControlKind::Fader:
            case ControlKind::Meter:
                return true;

            default:
                return false;
            }
        }

        // A control that is on or off rather than somewhere along a travel. Its plate is what
        // carries the state, which is the one place a hue is allowed to fill an area.
        bool IsSwitchKind(_In_ ControlKind kind) noexcept
        {
            switch (kind)
            {
            case ControlKind::Button:
            case ControlKind::Toggle:
            case ControlKind::Pad:
            case ControlKind::PageTab:
            case ControlKind::Lamp:
            case ControlKind::Lfo:
                return true;

            default:
                return false;
            }
        }

        // How many marks this control draws across its travel, and whether it draws any. Zero
        // means none.
        int32_t TickCountFor(_In_ Control const& control) noexcept
        {
            if (!control.Ticks.Show)
            {
                return 0;
            }

            return std::clamp(control.Ticks.Count, MinimumTickCount, MaximumTickCount);
        }

        // How many stops this control snaps to, or zero when it is smooth. The arithmetic that
        // turns a step or a list into a count lives in the document layer, so the marks drawn
        // here and the count the inspector shows can never disagree.
        int32_t DetentCountForControl(_In_ Control const& control) noexcept
        {
            return DetentStopCount(control);
        }

        projected::SurfaceControlRole RoleFor(_In_ ControlKind kind) noexcept
        {
            switch (kind)
            {
            case ControlKind::Pad:
            case ControlKind::Button:
            case ControlKind::PageTab:
            case ControlKind::PianoKeyboard:
                return projected::SurfaceControlRole::Button;

            case ControlKind::Toggle:
            case ControlKind::BeatClock:
            case ControlKind::Lfo:
                return projected::SurfaceControlRole::Toggle;

            case ControlKind::Label:
            case ControlKind::Readout:
            case ControlKind::Image:
            case ControlKind::Lamp:
            case ControlKind::Meter:
            case ControlKind::Panel:
            case ControlKind::TimeDisplay:
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
    CompositionLinearGradientBrush SurfaceRenderer::VerticalBrush(
        Compositor const& compositor,
        ThemeColor const& top,
        ThemeColor const& bottom,
        bool horizontal)
    {
        auto const key = (static_cast<uint64_t>(ColorKey(top)) << 32) |
            static_cast<uint64_t>(ColorKey(bottom)) | (horizontal ? 0x1ull : 0ull);

        auto const existing = m_gradients.find(key);

        if (existing != m_gradients.end())
        {
            return existing->second;
        }

        auto brush = compositor.CreateLinearGradientBrush();

        brush.StartPoint(horizontal ? float2{ 0.0f, 0.5f } : float2{ 0.5f, 0.0f });
        brush.EndPoint(horizontal ? float2{ 1.0f, 0.5f } : float2{ 0.5f, 1.0f });

        auto first = compositor.CreateColorGradientStop();
        first.Offset(0.0f);
        first.Color(ToColor(top));

        auto last = compositor.CreateColorGradientStop();
        last.Offset(1.0f);
        last.Color(ToColor(bottom));

        brush.ColorStops().Append(first);
        brush.ColorStops().Append(last);

        m_gradients.emplace(key, brush);

        return brush;
    }

    _Use_decl_annotations_
    CompositionLinearGradientBrush SurfaceRenderer::SheenBrush(
        Compositor const& compositor,
        ThemeColor const& color)
    {
        auto const key = 0x4000000000000000ull | static_cast<uint64_t>(ColorKey(color));

        auto const existing = m_gradients.find(key);

        if (existing != m_gradients.end())
        {
            return existing->second;
        }

        auto clear = color;
        clear.A = 0;

        auto brush = compositor.CreateLinearGradientBrush();

        brush.StartPoint(float2{ 0.5f, 0.0f });
        brush.EndPoint(float2{ 0.5f, 1.0f });

        auto top = compositor.CreateColorGradientStop();
        top.Offset(0.0f);
        top.Color(ToColor(color));

        // Gone before the middle, the way light falls off a curved edge. Past that it is the
        // plate's own color, so the bottom of the control is not lifted at all.
        auto knee = compositor.CreateColorGradientStop();
        knee.Offset(SheenFalloff);
        knee.Color(ToColor(clear));

        auto bottom = compositor.CreateColorGradientStop();
        bottom.Offset(1.0f);
        bottom.Color(ToColor(clear));

        brush.ColorStops().Append(top);
        brush.ColorStops().Append(knee);
        brush.ColorStops().Append(bottom);

        m_gradients.emplace(key, brush);

        return brush;
    }

    _Use_decl_annotations_
    CompositionBrush SurfaceRenderer::ShadowMaskFor(
        Compositor const& compositor,
        float width,
        float height,
        float cornerRadius,
        bool round)
    {
        // A rounded rectangle stretches through a nine grid, so every control that shares a
        // corner radius shares one offscreen. A circle cannot, because its corners are half its
        // size, so those are cached by size instead.
        auto const radius = round
            ? std::max(2.0f, std::min(width, height) / 2.0f)
            : std::max(0.5f, cornerRadius);

        auto const key = round
            ? (0x8000000000000000ull |
                (static_cast<uint64_t>(std::lround(width)) << 20) |
                static_cast<uint64_t>(std::lround(height)))
            : static_cast<uint64_t>(std::lround(radius * 4.0f));

        auto const existing = m_shadowMasks.find(key);

        if (existing != m_shadowMasks.end())
        {
            return existing->second;
        }

        auto const sourceWidth = round ? width : radius * 4.0f + 4.0f;
        auto const sourceHeight = round ? height : radius * 4.0f + 4.0f;

        auto source = compositor.CreateShapeVisual();
        source.Size(float2{ sourceWidth, sourceHeight });

        auto geometry = compositor.CreateRoundedRectangleGeometry();
        geometry.Size(float2{ sourceWidth, sourceHeight });
        geometry.CornerRadius(float2{ radius, radius });

        auto shape = compositor.CreateSpriteShape(geometry);
        shape.FillBrush(compositor.CreateColorBrush(winrt::Windows::UI::Colors::White()));

        source.Shapes().Append(shape);

        auto surface = compositor.CreateVisualSurface();
        surface.SourceVisual(source);
        surface.SourceSize(float2{ sourceWidth, sourceHeight });

        auto surfaceBrush = compositor.CreateSurfaceBrush(surface);

        CompositionBrush mask{ surfaceBrush };

        if (!round)
        {
            auto nine = compositor.CreateNineGridBrush();
            nine.Source(surfaceBrush);
            nine.SetInsets(radius + 1.0f);

            mask = nine;
        }

        // The offscreen is only rendered while the visual it reads from is alive.
        m_maskSources.push_back(source);
        m_shadowMasks.emplace(key, mask);

        return mask;
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

        // Kept so a control's own picture resolves against the same folder the background does.
        m_layoutFilePath = document.FilePath;

        if (host == nullptr || pageIndex >= document.Pages.size())
        {
            return;
        }

        auto const compositor = ElementCompositionPreview::GetElementVisual(host).Compositor();

        BuildBackground(document);

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
    void SurfaceRenderer::BuildBackground(LayoutDocument const& document)
    {
        m_background = nullptr;

        if (m_host == nullptr || document.BackgroundImage.empty())
        {
            return;
        }

        try
        {
            auto const path = BackgroundImagePath(document);

            if (path.empty())
            {
                return;
            }

            foundation::Uri const uri{ L"file:///" + winrt::hstring{ path } };

            media::Imaging::BitmapImage bitmap{};
            bitmap.UriSource(uri);

            // Tiling is a brush on a rectangle rather than an Image, because an Image has one
            // copy of the picture in it and no way to repeat it.
            if (document.BackgroundFitMode == BackgroundFit::Tiled)
            {
                winrt::Microsoft::UI::Xaml::Shapes::Rectangle tile{};

                tile.Width(document.PageWidth);
                tile.Height(document.PageHeight);
                tile.IsHitTestVisible(false);
                tile.Opacity(std::clamp(document.BackgroundOpacity, 0.0, 1.0));

                media::ImageBrush brush{};
                brush.ImageSource(bitmap);
                brush.Stretch(media::Stretch::None);
                brush.AlignmentX(media::AlignmentX::Left);
                brush.AlignmentY(media::AlignmentY::Top);

                tile.Fill(brush);

                controls::Canvas::SetLeft(tile, 0.0);
                controls::Canvas::SetTop(tile, 0.0);
                controls::Canvas::SetZIndex(tile, -1);

                m_host.Children().InsertAt(0, tile);
                m_background = tile;

                return;
            }

            controls::Image image{};

            image.Source(bitmap);
            image.IsHitTestVisible(false);
            image.Opacity(std::clamp(document.BackgroundOpacity, 0.0, 1.0));
            image.Width(document.PageWidth);
            image.Height(document.PageHeight);

            switch (document.BackgroundFitMode)
            {
            case BackgroundFit::Centered:
                image.Stretch(media::Stretch::None);
                break;

            case BackgroundFit::Stretch:
                image.Stretch(media::Stretch::Fill);
                break;

            default:
                image.Stretch(media::Stretch::Uniform);
                break;
            }

            controls::Canvas::SetLeft(image, 0.0);
            controls::Canvas::SetTop(image, 0.0);
            controls::Canvas::SetZIndex(image, -1);

            m_host.Children().InsertAt(0, image);
            m_background = image;
        }
        catch (...)
        {
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

        GlassControlElement element{};

        element.Width(width);
        element.Height(height);
        element.SurfaceName(winrt::hstring{ control.Label.empty() ? control.Id : control.Label });
        element.SurfaceRole(RoleFor(control.Kind));
        element.IsTabStop(RoleFor(control.Kind) != projected::SurfaceControlRole::Text);
        element.UseSystemFocusVisuals(true);

        // A grouping panel sits under the controls it frames. A finger landing on the empty part
        // of it should reach the deck rather than being swallowed by a frame.
        element.IsHitTestVisible(control.Kind != ControlKind::Panel);

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

        LayoutVisual(compositor, visual, control, theme);

        ElementCompositionPreview::SetElementChildVisual(element, visual.Root);

        m_visuals.push_back(std::move(visual));
        m_elements.push_back(element);
        m_controlIndexes.push_back(controlIndex);
        m_kinds.push_back(control.Kind);
        m_restValues.push_back(control.DefaultValue);
        m_restValuesY.push_back(control.DefaultValueY);
        m_returnsToRest.push_back(control.ReturnsToDefault);
        m_dragAxes.push_back(control.Drag);
        m_keyboards.push_back(control.Keyboard);
        m_velocityFromTouch.push_back(control.VelocityFromTouch);
        m_latches.push_back(control.Kind != ControlKind::Lfo || control.Lfo.Latching);
        m_turnDegrees.push_back(std::clamp(
            control.Turntable.DegreesForFullRange,
            MinimumTurntableDegrees,
            MaximumTurntableDegrees));
        m_pictures.push_back(nullptr);
        m_detentTexts.push_back(nullptr);
        m_beatTexts.push_back(nullptr);
        m_beatTextOffsets.push_back(0.0);
        m_tempoTexts.push_back(nullptr);
        m_tempoTextOffsets.push_back(0.0);
        m_elapsedTexts.push_back(nullptr);
        m_elapsedTextOffsets.push_back(0.0);
        m_elapsedOrigins.push_back(0);
        m_valueTexts.push_back(nullptr);
        m_valueOffsets.push_back(0.0);
        m_showValues.push_back(control.ShowValue);
        m_touched.push_back(false);
        m_labels.push_back(nullptr);
        m_labelOffsets.push_back(0.0);
        m_labelInsets.push_back(0.0);
        m_labelBoxInsets.push_back(0.0);
        m_labelBoxOffsets.push_back(0.0);
        m_labelBoxWidths.push_back(0.0);
        m_labelBoxHeights.push_back(0.0);
        m_values.push_back(control.DefaultValue);
        m_valuesY.push_back(control.DefaultValueY);
        m_litUntil.push_back(0);

        auto const itemIndex = m_visuals.size() - 1;

        SetValue(itemIndex, control.DefaultValue);
        SetValueY(itemIndex, control.DefaultValueY);
        winrt::get_self<winrt::midiglass::implementation::GlassControl>(element)
            ->SetValueDirect(control.DefaultValue);

        LayoutLabel(itemIndex, control, theme);
        LayoutValueText(itemIndex, control, theme);
        LayoutPicture(itemIndex, control);
        LayoutDetentValues(itemIndex, control, theme);
        LayoutBeatText(itemIndex, control, theme);
        LayoutElapsedText(itemIndex, control, theme);
    }

    // The number inside the control. There is no comp for this, so it follows the design sheet's
    // own rule for it: monospace, small, in the control's hue, centered along the bottom where
    // it cannot cross the plate's edge whatever shape the control is.
    _Use_decl_annotations_
    void SurfaceRenderer::LayoutValueText(size_t itemIndex, Control const& control, Theme const& theme)
    {
        if (itemIndex >= m_valueTexts.size())
        {
            return;
        }

        // A control with no value to show is most of them. UseTheme means while touched: a
        // number under every knob on a resting page is noise, and a number under the one being
        // held is the thing somebody is looking for.
        auto const wanted =
            control.ShowValue != ShowValueOverride::Never &&
            ShowsAValueReadout(control.Kind);

        if (!wanted)
        {
            if (m_valueTexts[itemIndex] != nullptr)
            {
                uint32_t index{ 0 };

                if (m_host != nullptr && m_host.Children().IndexOf(m_valueTexts[itemIndex], index))
                {
                    m_host.Children().RemoveAt(index);
                }

                m_valueTexts[itemIndex] = nullptr;
            }

            return;
        }

        if (m_valueTexts[itemIndex] == nullptr)
        {
            controls::TextBlock text{};

            text.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
            text.FontSize(10);
            text.TextAlignment(xaml::TextAlignment::Center);
            text.IsHitTestVisible(false);

            // The control already carries its value for assistive technology through its range
            // value pattern, so reading this as well would say everything twice.
            xaml::Automation::AutomationProperties::SetAccessibilityView(
                text, xaml::Automation::Peers::AccessibilityView::Raw);

            m_host.Children().Append(text);
            m_valueTexts[itemIndex] = text;
        }

        auto const colors = ResolveControlColors(control, theme);
        auto const& text = m_valueTexts[itemIndex];

        auto const width = std::max(control.Width, 4.0);
        auto const height = std::max(control.Height, 4.0);

        // Above an inside label, or where an inside label would have been.
        auto const labelInside =
            control.LabelPlaced == LabelPlacementOverride::Inside ||
            (control.LabelPlaced == LabelPlacementOverride::UseTheme &&
                theme.Labels == LabelPlacement::Inside);

        text.Width(width);
        text.Foreground(media::SolidColorBrush(ToColor(colors.Pipe)));

        // A two axis control shows both of its values, one to a line. One value under a
        // joystick says nothing about where the stick is.
        auto const lines = UsesTwoAxes(control.Kind) ? 2.0 : 1.0;

        m_valueOffsets[itemIndex] =
            height - (labelInside && !control.Label.empty() ? 34.0 : 19.0) - (lines - 1.0) * 12.0;

        controls::Canvas::SetLeft(text, control.X);
        controls::Canvas::SetTop(text, control.Y + m_valueOffsets[itemIndex]);

        RefreshValueText(itemIndex);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::RefreshValueText(size_t itemIndex) noexcept
    {
        if (itemIndex >= m_valueTexts.size() || m_valueTexts[itemIndex] == nullptr)
        {
            return;
        }

        try
        {
            auto const always = m_showValues[itemIndex] == ShowValueOverride::Always;
            auto const visible = always || m_touched[itemIndex];

            m_valueTexts[itemIndex].Visibility(
                visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (!visible)
            {
                return;
            }

            auto const controlIndex = ControlIndexOf(itemIndex);

            auto const describe = [&](ValueAxis axis, double value)
                {
                    return DescribeValue
                        ? DescribeValue(controlIndex, axis, value)
                        : std::to_wstring(static_cast<int32_t>(std::lround(value * 100.0))) + L" %";
                };

            auto const across = describe(ValueAxis::X, m_values[itemIndex]);

            if (!UsesTwoAxes(m_kinds[itemIndex]))
            {
                m_valueTexts[itemIndex].Text(winrt::hstring{ across });
                return;
            }

            // Two lines rather than one. A joystick is narrow, and a single number under one
            // says nothing about where the stick actually is.
            auto both = std::wstring{ resources::FormatString(L"TwoAxisValueAcrossFormat", across) };

            both += L'\n';
            both += resources::FormatString(
                L"TwoAxisValueDownFormat", describe(ValueAxis::Y, m_valuesY[itemIndex]));

            m_valueTexts[itemIndex].Text(winrt::hstring{ both });
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetTouched(size_t itemIndex, bool touched) noexcept
    {
        if (itemIndex >= m_touched.size())
        {
            return;
        }

        m_touched[itemIndex] = touched;

        // A ribbon has nothing riding it, so the light is the only thing that says where the
        // value is. It stays visible at rest and comes right up under a finger.
        if (itemIndex < m_visuals.size())
        {
            auto const& visual = m_visuals[itemIndex];

            if (!visual.RibbonGlow.empty() && visual.ValueShape != nullptr)
            {
                visual.ValueShape.Opacity(touched ? 1.0f : RibbonRestOpacity);
            }
        }

        RefreshValueText(itemIndex);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutVisual(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        Theme const& theme)
    {
        auto const width = static_cast<float>(std::max(control.Width, 4.0));
        auto const height = static_cast<float>(std::max(control.Height, 4.0));

        auto const colors = ResolveControlColors(control, theme);

        visual.Kind = control.Kind;
        visual.FeedbackHoldMilliseconds =
            control.Feedback.Enabled ? control.Feedback.HoldMilliseconds : 0;
        visual.Width = width;
        visual.Height = height;
        visual.HasThumb = false;
        visual.ThumbLength = 0.0f;

        // Rebuilt rather than reused: the track, the pipe and the arc all have their size baked
        // into their geometry, and there is nothing to tune once they exist.
        visual.PipeGeometry = nullptr;
        visual.ArcGeometry = nullptr;
        visual.PipeShape = nullptr;
        visual.ThumbGeometry = nullptr;
        visual.ThumbLineGeometry = nullptr;

        auto const round = IsRoundControl(control.Kind);

        auto const corner = round
            ? std::min(width, height) * 0.5f
            : static_cast<float>(std::min(
                static_cast<double>(theme.CornerRadius), std::min(width, height) / 2.0));

        if (visual.Root == nullptr)
        {
            visual.Root = compositor.CreateContainerVisual();

            visual.Elevation = compositor.CreateSpriteVisual();
            visual.Root.Children().InsertAtTop(visual.Elevation);

            // The bloom sits behind the plate. Changing a visual's opacity invalidates no layout,
            // which is what makes a page of blinking controls free.
            visual.Bloom = compositor.CreateSpriteVisual();
            visual.Bloom.Opacity(0.0f);
            visual.Root.Children().InsertAtTop(visual.Bloom);

            visual.Shape = compositor.CreateShapeVisual();
            visual.Root.Children().InsertAtTop(visual.Shape);

            visual.ValueShape = compositor.CreateShapeVisual();
            visual.Root.Children().InsertAtTop(visual.ValueShape);

            visual.Unavailable = compositor.CreateShapeVisual();
            visual.Unavailable.IsVisible(false);
            visual.Root.Children().InsertAtTop(visual.Unavailable);
        }
        else
        {
            visual.Shape.Shapes().Clear();
            visual.ValueShape.Shapes().Clear();
            visual.Unavailable.Shapes().Clear();

            // Not a shape but a whole visual, so clearing the shape collections does not reach
            // it. Without this a resize stacks a second grid behind the first.
            if (visual.Grid != nullptr)
            {
                visual.Root.Children().Remove(visual.Grid);
                visual.Grid = nullptr;
            }
        }

        visual.Root.Size(float2{ width, height });
        visual.Shape.Size(float2{ width, height });
        visual.ValueShape.Size(float2{ width, height });
        visual.Unavailable.Size(float2{ width, height });

        // The theme decides how a surface looks; one control can disagree with it. Outline drops
        // the plate, Solid fills it in the control's own hue, Bare drops both.
        auto const style = control.Style;

        auto plateColor = colors.Plate;
        auto rimColor = colors.Rim;

        if (style == ControlStyleOverride::Outline || style == ControlStyleOverride::Bare)
        {
            plateColor.A = 0;
        }
        else if (style == ControlStyleOverride::Solid)
        {
            plateColor = colors.Pipe;
            plateColor.A = 170;
        }

        if (style == ControlStyleOverride::Bare)
        {
            rimColor.A = 0;
        }

        // ---- the elevation shadow, cast through a mask so it is the shape of the control ----

        if (theme.PlateElevation > 0 && plateColor.A > 0)
        {
            auto shadow = compositor.CreateDropShadow();

            shadow.BlurRadius(3.0f);
            shadow.Offset(float3{ 0.0f, 1.0f, 0.0f });
            shadow.Color(winrt::Windows::UI::Colors::Black());
            shadow.Opacity(static_cast<float>(theme.PlateElevation) / 100.0f);
            shadow.Mask(ShadowMaskFor(compositor, width, height, corner, round));

            visual.Elevation.Size(float2{ width, height });
            visual.Elevation.Shadow(shadow);
            visual.Elevation.IsVisible(true);
        }
        else
        {
            visual.Elevation.Shadow(nullptr);
            visual.Elevation.IsVisible(false);
        }

        // ---- the glow: a real blur in the control's hue, shaped by the same mask ----

        if (colors.Bloom.A > 0)
        {
            visual.BloomShadow = compositor.CreateDropShadow();

            visual.BloomShadow.BlurRadius(GlowBlurRadius);
            visual.BloomShadow.Offset(float3{ 0.0f, 0.0f, 0.0f });
            visual.BloomShadow.Color(ToColor(colors.Bloom));
            visual.BloomShadow.Opacity(1.0f);
            visual.BloomShadow.Mask(ShadowMaskFor(compositor, width, height, corner, round));

            visual.Bloom.Size(float2{ width, height });
            visual.Bloom.Offset(float3{ 0.0f, 0.0f, 0.0f });
            visual.Bloom.Shadow(visual.BloomShadow);
        }
        else
        {
            visual.BloomShadow = nullptr;
            visual.Bloom.Shadow(nullptr);
            visual.Bloom.Size(float2{ width, height });
        }

        // ---- the plate, its sheen and its rim ----

        auto plateGeometry = compositor.CreateRoundedRectangleGeometry();
        plateGeometry.Size(float2{ width - 1.0f, height - 1.0f });
        plateGeometry.Offset(float2{ 0.5f, 0.5f });
        plateGeometry.CornerRadius(float2{ corner, corner });

        visual.PlateShape = compositor.CreateSpriteShape(plateGeometry);
        visual.PlateShape.FillBrush(BrushFor(compositor, plateColor));

        visual.IsSwitch = IsSwitchKind(control.Kind);
        visual.PlateOffBrush = BrushFor(compositor, plateColor);
        visual.RimOffBrush = rimColor.A != 0 ? BrushFor(compositor, rimColor).as<CompositionBrush>() : nullptr;

        // What the plate becomes while it is on: the hue carried by the plate itself, top
        // brighter than bottom, with the rim coming right up. Built once so a switch changing
        // state is two property sets rather than a rebuild.
        if (visual.IsSwitch && plateColor.A > 0)
        {
            visual.PlateOnBrush = VerticalBrush(compositor, colors.OnPlate, colors.OnPlateEnd, false);
            visual.RimOnBrush = BrushFor(compositor, colors.OnRim);
        }
        else
        {
            visual.PlateOnBrush = nullptr;
            visual.RimOnBrush = nullptr;
        }

        if (rimColor.A != 0)
        {
            visual.PlateShape.StrokeBrush(BrushFor(compositor, rimColor));
            visual.PlateShape.StrokeThickness(1.0f);
        }

        visual.Shape.Shapes().Append(visual.PlateShape);

        // A wash of light down the top of the plate, fading out before the middle. It is the
        // one thing that makes a plate read as a raised piece of glass rather than a filled
        // rectangle, and it costs one shape.
        if (colors.Sheen.A > 0 && plateColor.A > 0)
        {
            // The plate's own geometry, not a rectangle laid over it: a round control's sheen
            // has to follow its edge, and a rectangle spills out of a circle at the corners.
            auto sheenGeometry = compositor.CreateRoundedRectangleGeometry();
            sheenGeometry.Size(float2{ width - 1.0f, height - 1.0f });
            sheenGeometry.Offset(float2{ 0.5f, 0.5f });
            sheenGeometry.CornerRadius(float2{ corner, corner });

            auto sheenShape = compositor.CreateSpriteShape(sheenGeometry);
            sheenShape.FillBrush(SheenBrush(compositor, colors.Sheen));

            visual.Shape.Shapes().Append(sheenShape);
        }

        // ---- struck through, for when the device is not here ----

        {
            auto hatchClip = compositor.CreateRoundedRectangleGeometry();
            hatchClip.Size(float2{ width, height });
            hatchClip.CornerRadius(float2{ corner, corner });

            visual.Unavailable.Clip(compositor.CreateGeometricClip(hatchClip));

            auto const ink = ReadableInk(m_deck);

            auto stripe = ink;
            stripe.A = 26;

            // Diagonals at 45 degrees, drawn past both ends so the clip does the shaping.
            auto const reach = width + height;

            for (float offset = -height; offset < width; offset += HatchSpacing)
            {
                auto line = compositor.CreateLineGeometry();
                line.Start(float2{ offset, height });
                line.End(float2{ offset + reach, height - reach });

                auto lineShape = compositor.CreateSpriteShape(line);
                lineShape.StrokeBrush(BrushFor(compositor, stripe));
                lineShape.StrokeThickness(HatchThickness);

                visual.Unavailable.Shapes().Append(lineShape);
            }
        }

        if (DrawsItsOwnValue(control.Kind))
        {
            switch (control.Kind)
            {
            case ControlKind::XYPad:
            case ControlKind::Joystick:
                LayoutTwoAxis(compositor, visual, control, colors, theme, width, height);
                break;

            case ControlKind::Ribbon:
                LayoutRibbon(compositor, visual, control, colors, theme, width, height);
                break;

            case ControlKind::PianoKeyboard:
                LayoutKeyboard(compositor, visual, control, colors, width, height);
                break;

            case ControlKind::BeatClock:
                LayoutClock(compositor, visual, control, colors, width, height);
                break;

            case ControlKind::Lfo:
                LayoutLfo(compositor, visual, control, colors, width, height);
                break;

            case ControlKind::Turntable:
                LayoutTurntable(compositor, visual, control, colors, width, height);
                break;

            default:
                break;
            }

            return;
        }

        if (DrawsAPipe(control.Kind))
        {
            if (round)
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

                visual.ArcGeometry = compositor.CreateEllipseGeometry();
                visual.ArcGeometry.Radius(float2{ radius, radius });
                visual.ArcGeometry.Center(float2{ width * 0.5f, height * 0.5f });
                visual.ArcGeometry.TrimOffset(KnobTrimOffset);
                visual.ArcGeometry.TrimStart(0.0f);
                visual.ArcGeometry.TrimEnd(0.0f);

                visual.PipeShape = compositor.CreateSpriteShape(visual.ArcGeometry);
                visual.PipeShape.StrokeBrush(BrushFor(compositor, colors.Pipe));
                visual.PipeShape.StrokeThickness(KnobArcThickness);

                // Bigwig's ring of lamps is the same arc with a repeating gap laid over it, so a
                // ringed knob is still one shape rather than thirty. Below the theme's own floor
                // the lamps stop separating and it falls back to the solid arc on its own.
                if (UsesLampRing(theme, control.Width, control.Height))
                {
                    auto const circumference = 2.0f * 3.14159265f * radius * (KnobSweepDegrees / 360.0f);
                    auto const lamps = static_cast<float>(std::max(2, theme.LampCount));
                    auto const period = circumference / lamps / KnobArcThickness;

                    auto const dash = std::max(0.2f, period * LampDutyCycle);
                    auto const gap = std::max(0.1f, period - dash);

                    for (auto const& shape : { trackShape, visual.PipeShape })
                    {
                        shape.StrokeDashArray().Append(dash);
                        shape.StrokeDashArray().Append(gap);
                        shape.StrokeDashCap(CompositionStrokeCap::Flat);
                    }
                }

                visual.Shape.Shapes().Append(trackShape);
                visual.ValueShape.Shapes().Append(visual.PipeShape);

                // ---- the pointer, the center dot and the detent mark ----

                auto const center = float2{ width * 0.5f, height * 0.5f };
                auto const dial = std::min(width, height);

                auto const pointerWidth = std::max(2.0f, dial * PointerWidthFraction);
                auto const pointerLength = dial * PointerLengthFraction;

                // Starts just inside the track rather than at a fixed fraction: the comp hangs
                // its arc outside the dial and this one runs inside it, so a fixed inset puts
                // the pointer straight through the arc.
                auto const pointerTop =
                    center.y - (radius - KnobArcThickness * 0.5f - PointerClearance);

                auto pointerGeometry = compositor.CreateRoundedRectangleGeometry();
                pointerGeometry.Size(float2{ pointerWidth, pointerLength });
                pointerGeometry.Offset(float2{ center.x - pointerWidth * 0.5f, pointerTop });
                pointerGeometry.CornerRadius(float2{ pointerWidth * 0.5f, pointerWidth * 0.5f });

                visual.PointerShape = compositor.CreateSpriteShape(pointerGeometry);
                visual.PointerShape.FillBrush(BrushFor(compositor, colors.Pointer));
                visual.PointerShape.CenterPoint(center);

                visual.ValueShape.Shapes().Append(visual.PointerShape);

                auto const dotSize = std::max(3.0f, dial * CenterDotFraction);

                auto dotGeometry = compositor.CreateEllipseGeometry();
                dotGeometry.Radius(float2{ dotSize * 0.5f, dotSize * 0.5f });
                dotGeometry.Center(center);

                auto dotShape = compositor.CreateSpriteShape(dotGeometry);
                dotShape.FillBrush(BrushFor(compositor, colors.Marks));

                visual.ValueShape.Shapes().Append(dotShape);

                // A knob with stops gets a mark at every one of them, so the middle position on
                // a six way switch can be found without watching the readout. A knob with no
                // stops gets whatever marks the customer asked for around its arc.
                auto const detents = DetentCountForControl(control);
                auto const marks = detents > 1 ? detents : TickCountFor(control);

                if (marks > 1)
                {
                    auto const outer = radius + KnobArcThickness * 0.5f + 1.0f;
                    auto const sweep = KnobSweepDegrees * 3.14159265f / 180.0f;
                    auto const start = (KnobStartAngle - 90.0f) * 3.14159265f / 180.0f;

                    for (int32_t mark = 0; mark < marks; ++mark)
                    {
                        auto const fraction =
                            static_cast<float>(mark) / static_cast<float>(marks - 1);

                        auto const angle = start + sweep * fraction;

                        auto const from = float2{
                            center.x + std::cos(angle) * outer,
                            center.y + std::sin(angle) * outer };

                        auto const to = float2{
                            center.x + std::cos(angle) * (outer + DetentTickLength),
                            center.y + std::sin(angle) * (outer + DetentTickLength) };

                        auto line = compositor.CreateLineGeometry();
                        line.Start(from);
                        line.End(to);

                        auto markShape = compositor.CreateSpriteShape(line);
                        markShape.StrokeBrush(BrushFor(compositor, colors.Marks));
                        markShape.StrokeThickness(1.0f);

                        visual.Shape.Shapes().Append(markShape);
                    }
                }
            }
            else
            {
                auto const vertical = IsTallControl(control.Kind, control.Width, control.Height);

                visual.Vertical = vertical;

                auto const travels = DrawsATrack(control.Kind);

                // A button has no travel, so its value is a strip along one edge rather than a
                // slot through the middle. The theme says which edge.
                auto const strip = !travels;

                if (strip && theme.ValueStrip == ValueStripPlacement::None)
                {
                    return;
                }

                auto const slot = travels
                    ? std::max(6.0f, std::min(width, height) * 0.2f)
                    : StripThickness;

                auto const stripTop = theme.ValueStrip == ValueStripPlacement::Top;

                auto const trackX = vertical ? (width * 0.5f - slot * 0.5f) : PipeInset;
                auto const trackY = vertical
                    ? PipeInset
                    : (strip
                        ? (stripTop ? StripEdgeInset : height - StripEdgeInset - slot)
                        : height - PipeInset - slot);

                auto const trackW = vertical ? slot : (width - PipeInset * 2);
                auto const trackH = vertical ? (height - PipeInset * 2) : slot;

                auto trackGeometry = compositor.CreateRoundedRectangleGeometry();
                trackGeometry.Size(float2{ std::max(trackW, 1.0f), std::max(trackH, 1.0f) });
                trackGeometry.Offset(float2{ trackX, trackY });
                trackGeometry.CornerRadius(float2{ slot * 0.5f, slot * 0.5f });

                auto trackShape = compositor.CreateSpriteShape(trackGeometry);

                // The unlit part of a button's strip is its own hue turned right down, the way
                // an unlit lamp still shows you where the lamp is. A fader's is the theme's
                // track color, because a fader slot is a groove in the surface.
                if (strip)
                {
                    auto dim = colors.Pipe;
                    dim.A = static_cast<uint8_t>(std::lround(dim.A * 0.22));
                    trackShape.FillBrush(BrushFor(compositor, dim));
                }
                else
                {
                    trackShape.FillBrush(BrushFor(compositor, colors.Track));

                    // The groove has a hairline down its lit edge, which is what stops it
                    // reading as a painted stripe.
                    trackShape.StrokeBrush(BrushFor(compositor, colors.Sheen));
                    trackShape.StrokeThickness(1.0f);
                }

                visual.Shape.Shapes().Append(trackShape);

                // Tick marks across the travel, so a fader has somewhere to be other than the
                // two ends. Two short marks either side of the slot rather than one line across
                // it: a line through the value bar reads as a defect in the bar.
                //
                // Stops win over marks where a control has both. Somebody who set six positions
                // wants to see six, not five evenly spaced ones that do not line up with them.
                auto const detents = DetentCountForControl(control);
                auto const tickCount = detents > 1 ? detents : TickCountFor(control);

                if (travels && tickCount > 1)
                {
                    auto const cross = vertical ? width : height;
                    auto const markLength = (cross * FaderTickSpan - slot) * 0.5f;

                    auto const trackStart = vertical ? trackY : trackX;
                    auto const trackRun = vertical ? trackH : trackW;

                    if (markLength >= 2.0f)
                    {
                        // Not "near" and "far": both are macros out of windows.h.
                        auto const leading = (cross - cross * FaderTickSpan) * 0.5f;
                        auto const trailing = (cross + slot) * 0.5f;

                        for (int32_t tick = 0; tick < tickCount; ++tick)
                        {
                            auto const along = std::clamp(
                                trackStart + trackRun * static_cast<float>(tick) /
                                    static_cast<float>(tickCount - 1),
                                0.0f,
                                (vertical ? height : width) - 1.0f);

                            for (auto const side : { leading, trailing })
                            {
                                auto tickGeometry = compositor.CreateRoundedRectangleGeometry();
                                tickGeometry.Size(vertical
                                    ? float2{ markLength, 1.0f }
                                    : float2{ 1.0f, markLength });
                                tickGeometry.Offset(vertical
                                    ? float2{ side, along }
                                    : float2{ along, side });

                                auto tickShape = compositor.CreateSpriteShape(tickGeometry);
                                tickShape.FillBrush(BrushFor(compositor, colors.Marks));

                                visual.Shape.Shapes().Append(tickShape);
                            }
                        }
                    }
                }

                visual.PipeGeometry = compositor.CreateRoundedRectangleGeometry();
                visual.PipeGeometry.CornerRadius(float2{ slot * 0.5f, slot * 0.5f });
                visual.PipeGeometry.Size(float2{ vertical ? trackW : 0.0f, vertical ? 0.0f : trackH });
                visual.PipeGeometry.Offset(float2{ trackX, vertical ? (trackY + trackH) : trackY });

                // The light around the bar: concentric strokes of the bar's OWN geometry, each
                // one the step between two points on a gaussian, widest and faintest first. A
                // blurred drop shadow was the obvious way to do this and it came out as a bright
                // knuckle halfway down the bar with no light at all along the rest of it.
                // Strokes also track the value for free, because they share the bar's geometry.
                //
                // The curve is measured off the comp rather than guessed: at the bar's edge the
                // halo is a little under half the bar's own strength, and it is gone by about
                // one slot width out. Evenly spaced rings of similar weight instead give a flat
                // plateau with a cliff at the end of it, which reads as a hard edge.
                if (colors.Bloom.A > 0)
                {
                    auto const reach = slot * GlowReachFraction;
                    auto const peak = GlowPeakAlpha * theme.GlowStrength / 100.0;

                    auto previous = 0.0;

                    for (int32_t ring = 0; ring < GlowRingCount; ++ring)
                    {
                        // Outside in: 1 at the far edge of the halo, 0 at the bar.
                        auto const t = 1.0 - static_cast<double>(ring) / GlowRingCount;
                        auto const cumulative = peak * std::exp(-GlowTightness * t * t);
                        auto const step = cumulative - previous;

                        previous = cumulative;

                        if (step < GlowMinimumStep)
                        {
                            continue;
                        }

                        auto glow = colors.Pipe;
                        glow.A = static_cast<uint8_t>(std::clamp(std::lround(255.0 * step), 0L, 255L));

                        auto glowShape = compositor.CreateSpriteShape(visual.PipeGeometry);
                        glowShape.StrokeBrush(BrushFor(compositor, glow));

                        // A stroke straddles the path, so half of it is the reach. Adding the
                        // slot width here as well put every ring the same six pixels clear of
                        // the bar, which is what made the halo a flat plateau with a cliff.
                        glowShape.StrokeThickness(static_cast<float>(reach * t * 2.0));

                        visual.ValueShape.Shapes().Append(glowShape);
                    }
                }

                visual.PipeShape = compositor.CreateSpriteShape(visual.PipeGeometry);

                // Brightest where the value is, falling away behind it. On a vertical fader the
                // value is at the top of the fill, so the gradient runs the other way.
                visual.PipeShape.FillBrush(colors.PipeEnd == colors.Pipe
                    ? BrushFor(compositor, colors.Pipe).as<CompositionBrush>()
                    : VerticalBrush(
                        compositor,
                        vertical ? colors.Pipe : colors.PipeEnd,
                        vertical ? colors.PipeEnd : colors.Pipe,
                        !vertical).as<CompositionBrush>());

                visual.ValueShape.Shapes().Append(visual.PipeShape);

                visual.TrackOrigin = vertical ? trackY : trackX;
                visual.TrackLength = vertical ? trackH : trackW;
                visual.PipeThickness = vertical ? trackW : trackH;
                visual.PipeCrossOffset = vertical ? trackX : trackY;
                // ---- the cap ----

                if (travels && theme.Thumb != ThumbStyle::None && std::min(width, height) >= MinimumThumbSize)
                {
                    auto const cross = vertical ? width : height;

                    auto const thumbSpan = cross * ThumbSpanFraction;
                    auto const thumbThickness = std::min(
                        thumbSpan * ThumbAspect,
                        visual.TrackLength * MaximumThumbShareOfTravel);

                    visual.HasThumb = true;
                    visual.ThumbLength = thumbThickness;
                    visual.ThumbSpan = thumbSpan;
                    visual.ThumbInset = (cross - thumbSpan) * 0.5f;
                    visual.ThumbLineLength = thumbSpan * ThumbLineSpanFraction;
                    visual.ThumbLineThickness = std::max(1.0f, thumbThickness * ThumbLineAspect);

                    auto const thumbCorner = thumbThickness * ThumbCornerFraction;

                    visual.ThumbGeometry = compositor.CreateRoundedRectangleGeometry();
                    visual.ThumbGeometry.CornerRadius(float2{ thumbCorner, thumbCorner });
                    visual.ThumbGeometry.Size(vertical
                        ? float2{ thumbSpan, thumbThickness }
                        : float2{ thumbThickness, thumbSpan });

                    auto thumbShape = compositor.CreateSpriteShape(visual.ThumbGeometry);

                    thumbShape.FillBrush(colors.Thumb == colors.ThumbEnd
                        ? BrushFor(compositor, colors.Thumb).as<CompositionBrush>()
                        : VerticalBrush(compositor, colors.Thumb, colors.ThumbEnd, !vertical)
                            .as<CompositionBrush>());

                    visual.ValueShape.Shapes().Append(thumbShape);

                    // The hairline of hue through a neutral cap, which is what tells you which
                    // control you are holding when six of them are side by side.
                    if (colors.ThumbLine.A > 0)
                    {
                        auto const lineCorner = visual.ThumbLineThickness * 0.5f;

                        visual.ThumbLineGeometry = compositor.CreateRoundedRectangleGeometry();
                        visual.ThumbLineGeometry.CornerRadius(float2{ lineCorner, lineCorner });
                        visual.ThumbLineGeometry.Size(vertical
                            ? float2{ visual.ThumbLineLength, visual.ThumbLineThickness }
                            : float2{ visual.ThumbLineThickness, visual.ThumbLineLength });

                        auto lineShape = compositor.CreateSpriteShape(visual.ThumbLineGeometry);
                        lineShape.FillBrush(BrushFor(compositor, colors.ThumbLine));

                        visual.ValueShape.Shapes().Append(lineShape);
                    }
                }
            }
        }
    }

    // Created, moved, retexted or taken away, whichever the control now needs.
    _Use_decl_annotations_
    void SurfaceRenderer::LayoutLabel(size_t itemIndex, Control const& control, Theme const& theme)
    {
        if (itemIndex >= m_labels.size())
        {
            return;
        }

        auto const height = std::max(control.Height, 4.0);
        auto const width = std::max(control.Width, 4.0);

        // The theme says where labels go; one control can disagree with it. A theme only knows
        // the three original answers, so an override that is one of the newer ones stands on
        // its own rather than being folded back into them.
        auto placed = control.LabelPlaced;

        if (placed == LabelPlacementOverride::UseTheme)
        {
            placed = theme.Labels == LabelPlacement::Inside ? LabelPlacementOverride::Inside
                : theme.Labels == LabelPlacement::Below ? LabelPlacementOverride::Below
                : LabelPlacementOverride::None;
        }

        // A group's caption belongs at the top of the frame, the way every group box in every
        // application puts it.
        auto const isPanel = control.Kind == ControlKind::Panel;

        auto const wanted = placed != LabelPlacementOverride::None && !control.Label.empty();

        if (!wanted)
        {
            if (m_labels[itemIndex] != nullptr)
            {
                uint32_t index{ 0 };

                if (m_host != nullptr && m_host.Children().IndexOf(m_labels[itemIndex], index))
                {
                    m_host.Children().RemoveAt(index);
                }

                m_labels[itemIndex] = nullptr;
            }

            return;
        }

        if (m_labels[itemIndex] == nullptr)
        {
            controls::TextBlock label{};

            label.IsHitTestVisible(false);

            // The control beside it already carries the name, so a screen reader reading this as
            // well would say everything twice.
            xaml::Automation::AutomationProperties::SetAccessibilityView(
                label, xaml::Automation::Peers::AccessibilityView::Raw);

            m_host.Children().Append(label);
            m_labels[itemIndex] = label;
        }

        auto const colors = ResolveControlColors(control, theme);
        auto const& label = m_labels[itemIndex];
        auto const& look = control.LabelLook;

        label.Text(winrt::hstring{ control.Label });

        // ---- type ----

        label.FontSize(look.FontSize > 0.0 ? look.FontSize : 12.0);

        label.FontFamily(look.FontFamily.empty()
            ? media::FontFamily{ L"Segoe UI Variable Text" }
            : media::FontFamily{ look.FontFamily });

        label.FontWeight(winrt::Windows::UI::Text::FontWeight{
            static_cast<uint16_t>(look.FontWeight > 0 ? look.FontWeight : 400) });

        label.FontStyle(look.Italic
            ? winrt::Windows::UI::Text::FontStyle::Italic
            : winrt::Windows::UI::Text::FontStyle::Normal);

        label.TextDecorations(look.Underline
            ? winrt::Windows::UI::Text::TextDecorations::Underline
            : winrt::Windows::UI::Text::TextDecorations::None);

        // Wrapping on by default: a fader is narrower than most of the words people put under
        // one, and a word cut off in the middle is worse than a second line.
        label.TextWrapping(look.Wrap ? xaml::TextWrapping::Wrap : xaml::TextWrapping::NoWrap);
        label.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

        ThemeColor ink{ colors.Label };

        if (!look.Color.empty())
        {
            ThemeColor parsed{};

            if (TryParseColor(look.Color, parsed))
            {
                ink = parsed;
            }
        }

        label.Foreground(media::SolidColorBrush(ToColor(ink)));

        // ---- the box the text sits in ----

        // An explicit box wins over everything: the customer dragged the handles, so the text
        // goes where they put it and anything that does not fit is trimmed rather than moved.
        auto const custom = look.HasBox();

        auto const vertical = !custom &&
            (placed == LabelPlacementOverride::VerticalLeft ||
             placed == LabelPlacementOverride::VerticalRight);

        // Above 100 the label is centered on the control and spills either side, which is what
        // makes a readable caption possible under a forty pixel fader. Along the side of a
        // control the percentage governs its length instead.
        auto const along = vertical ? height : width;
        auto const boxLength = std::max(along * std::clamp(look.WidthPercent, 10.0, 400.0) / 100.0, 4.0);

        label.Width(custom ? look.BoxWidth : (isPanel ? std::max(width - 16.0, 4.0) : boxLength));

        label.TextAlignment(isPanel ? xaml::TextAlignment::Left : xaml::TextAlignment::Center);

        // ---- where it sits ----

        auto const lineHeight = (look.FontSize > 0.0 ? look.FontSize : 12.0) + 5.0;

        double offsetX{ 0.0 };
        double offsetY{ 0.0 };

        if (custom)
        {
            label.RenderTransform(nullptr);
            label.Height(look.BoxHeight);

            // An ellipsis needs a line budget: a text block given a height clips at it silently,
            // which leaves a word cut in half across the bottom and no sign that anything is
            // missing. Counting the lines that fit is what turns that into "Lead Synth Vol…".
            label.MaxLines(std::max(1, static_cast<int32_t>(look.BoxHeight / lineHeight)));

            offsetX = look.BoxX;
            offsetY = look.BoxY;
        }
        else if (isPanel)
        {
            label.Height(std::numeric_limits<double>::quiet_NaN());
            label.MaxLines(0);

            offsetX = 8.0;
            offsetY = 5.0;
        }
        else if (vertical)
        {
            // Rotated about its own center, then nudged so the resulting strip lands beside the
            // control rather than across it. Left reads bottom to top, right reads top to
            // bottom, which is how the words on a mixer's channel strip run.
            media::RotateTransform rotate{};
            rotate.Angle(placed == LabelPlacementOverride::VerticalLeft ? -90.0 : 90.0);
            rotate.CenterX(boxLength * 0.5);
            rotate.CenterY(lineHeight * 0.5);

            label.RenderTransform(rotate);
            label.Height(std::numeric_limits<double>::quiet_NaN());
            label.MaxLines(0);

            offsetX = placed == LabelPlacementOverride::VerticalLeft
                ? -(boxLength * 0.5) - (lineHeight * 0.5)
                : width - (boxLength * 0.5) + (lineHeight * 0.5);

            offsetY = (height - boxLength) * 0.5 + (boxLength - lineHeight) * 0.5;
        }
        else
        {
            label.RenderTransform(nullptr);
            label.Height(std::numeric_limits<double>::quiet_NaN());
            label.MaxLines(0);

            offsetX = (width - boxLength) * 0.5;

            switch (placed)
            {
            case LabelPlacementOverride::Above:
                offsetY = -lineHeight - 2.0;
                break;

            case LabelPlacementOverride::Below:
                offsetY = height + 2.0;
                break;

            case LabelPlacementOverride::InsideTop:
                offsetY = 4.0;
                break;

            case LabelPlacementOverride::InsideCenter:
                offsetY = (height - lineHeight) * 0.5;
                break;

            default:
                // Inside and InsideBottom are the same thing, and the one the themes mean.
                offsetY = height - lineHeight - 4.0;
                break;
            }
        }

        m_labelOffsets[itemIndex] = offsetY;
        m_labelInsets[itemIndex] = offsetX;

        // What the editor draws handles around. A rotated label occupies the strip its rotation
        // lands on, not the unrotated run of text, so the two are swapped for a vertical one.
        if (itemIndex < m_labelBoxWidths.size())
        {
            m_labelBoxWidths[itemIndex] = custom ? look.BoxWidth : (vertical ? lineHeight : label.Width());
            m_labelBoxHeights[itemIndex] = custom ? look.BoxHeight : (vertical ? boxLength : lineHeight);

            if (vertical)
            {
                // The rotation moves the painted strip away from the offset the text block was
                // placed at, so the box the customer sees starts somewhere else.
                m_labelBoxInsets[itemIndex] = offsetX + (boxLength - lineHeight) * 0.5;
                m_labelBoxOffsets[itemIndex] = offsetY - (boxLength - lineHeight) * 0.5;
            }
            else
            {
                m_labelBoxInsets[itemIndex] = offsetX;
                m_labelBoxOffsets[itemIndex] = offsetY;
            }
        }

        controls::Canvas::SetLeft(label, control.X + offsetX);
        controls::Canvas::SetTop(label, control.Y + offsetY);
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::TryGetLabelBox(
        size_t itemIndex,
        double& x,
        double& y,
        double& width,
        double& height) const noexcept
    {
        x = 0.0;
        y = 0.0;
        width = 0.0;
        height = 0.0;

        if (itemIndex >= m_labels.size() ||
            m_labels[itemIndex] == nullptr ||
            itemIndex >= m_labelBoxWidths.size())
        {
            return false;
        }

        x = m_labelBoxInsets[itemIndex];
        y = m_labelBoxOffsets[itemIndex];
        width = m_labelBoxWidths[itemIndex];
        height = m_labelBoxHeights[itemIndex];

        return width > 0.0 && height > 0.0;
    }

    _Use_decl_annotations_
    void SurfaceRenderer::ResizeItem(size_t itemIndex, Control const& control, Theme const& theme) noexcept
    {
        if (itemIndex >= m_visuals.size() || m_elements[itemIndex] == nullptr || m_host == nullptr)
        {
            return;
        }

        try
        {
            auto const compositor = ElementCompositionPreview::GetElementVisual(m_host).Compositor();
            auto const& element = m_elements[itemIndex];

            element.Width(std::max(control.Width, 4.0));
            element.Height(std::max(control.Height, 4.0));

            controls::Canvas::SetLeft(element, control.X);
            controls::Canvas::SetTop(element, control.Y);

            LayoutVisual(compositor, m_visuals[itemIndex], control, theme);
            SetValue(itemIndex, m_values[itemIndex]);
            SetValueY(itemIndex, m_valuesY[itemIndex]);
            LayoutLabel(itemIndex, control, theme);
            LayoutValueText(itemIndex, control, theme);
            LayoutPicture(itemIndex, control);
            LayoutDetentValues(itemIndex, control, theme);
            LayoutBeatText(itemIndex, control, theme);
            LayoutElapsedText(itemIndex, control, theme);
        }
        catch (...)
        {
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

        // Before the children go, or every video on the page keeps decoding into nothing.
        for (auto const& picture : m_pictures)
        {
            if (picture != nullptr)
            {
                ClosePicture(picture);
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
        m_restValues.clear();
        m_restValuesY.clear();
        m_returnsToRest.clear();
        m_dragAxes.clear();
        m_keyboards.clear();
        m_velocityFromTouch.clear();
        m_latches.clear();
        m_turnDegrees.clear();
        m_pictures.clear();
        m_detentTexts.clear();
        m_beatTexts.clear();
        m_beatTextOffsets.clear();
        m_tempoTexts.clear();
        m_tempoTextOffsets.clear();
        m_elapsedTexts.clear();
        m_elapsedTextOffsets.clear();
        m_elapsedOrigins.clear();

        if (m_elapsedTimer != nullptr)
        {
            m_elapsedTimer.Stop();
            m_elapsedTimer = nullptr;
        }
        m_valueTexts.clear();
        m_valueOffsets.clear();
        m_showValues.clear();
        m_touched.clear();
        m_labels.clear();
        m_labelOffsets.clear();
        m_labelInsets.clear();
        m_labelBoxInsets.clear();
        m_labelBoxOffsets.clear();
        m_labelBoxWidths.clear();
        m_labelBoxHeights.clear();
        m_background = nullptr;
        m_values.clear();
        m_valuesY.clear();
        m_litUntil.clear();

        if (m_flashTimer != nullptr)
        {
            m_flashTimer.Stop();
            m_flashTimer = nullptr;
        }
        m_brushes.clear();
        m_gradients.clear();
        m_shadowMasks.clear();
        m_maskSources.clear();
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
    DragAxis SurfaceRenderer::DragAxisAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_dragAxes.size() ? m_dragAxes[itemIndex] : DragAxis::Vertical;
    }

    _Use_decl_annotations_
    KeyboardSpec const& SurfaceRenderer::KeyboardAt(size_t itemIndex) const noexcept
    {
        static KeyboardSpec const none{};

        return itemIndex < m_keyboards.size() ? m_keyboards[itemIndex] : none;
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::VelocityFromTouchAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_velocityFromTouch.size() && m_velocityFromTouch[itemIndex];
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::LatchesAt(size_t itemIndex) const noexcept
    {
        return itemIndex >= m_latches.size() || m_latches[itemIndex];
    }

    _Use_decl_annotations_
    double SurfaceRenderer::TurnDegreesAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_turnDegrees.size() ? m_turnDegrees[itemIndex] : 180.0;
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::ReturnsToRestAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_returnsToRest.size() && m_returnsToRest[itemIndex];
    }

    _Use_decl_annotations_
    double SurfaceRenderer::RestValueAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_restValues.size() ? m_restValues[itemIndex] : 0.0;
    }

    _Use_decl_annotations_
    double SurfaceRenderer::RestValueYAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_restValuesY.size() ? m_restValuesY[itemIndex] : 0.0;
    }

    _Use_decl_annotations_
    double SurfaceRenderer::ValueYAt(size_t itemIndex) const noexcept
    {
        return itemIndex < m_valuesY.size() ? m_valuesY[itemIndex] : 0.0;
    }

    _Use_decl_annotations_
    void SurfaceRenderer::MoveItem(size_t itemIndex, double x, double y) noexcept
    {
        if (itemIndex >= m_elements.size() || m_elements[itemIndex] == nullptr)
        {
            return;
        }

        // The composition visuals are a child visual of the element, so moving the element moves
        // everything drawn inside it. The label is a separate child of the host and has to be
        // carried along, or it sits where the control used to be until the next rebuild.
        controls::Canvas::SetLeft(m_elements[itemIndex], x);
        controls::Canvas::SetTop(m_elements[itemIndex], y);

        if (itemIndex < m_labels.size() && m_labels[itemIndex] != nullptr)
        {
            controls::Canvas::SetLeft(m_labels[itemIndex], x + m_labelInsets[itemIndex]);
            controls::Canvas::SetTop(m_labels[itemIndex], y + m_labelOffsets[itemIndex]);
        }

        if (itemIndex < m_valueTexts.size() && m_valueTexts[itemIndex] != nullptr)
        {
            controls::Canvas::SetLeft(m_valueTexts[itemIndex], x);
            controls::Canvas::SetTop(m_valueTexts[itemIndex], y + m_valueOffsets[itemIndex]);
        }

        if (itemIndex < m_pictures.size() && m_pictures[itemIndex] != nullptr)
        {
            controls::Canvas::SetLeft(m_pictures[itemIndex], x);
            controls::Canvas::SetTop(m_pictures[itemIndex], y);
        }

        if (itemIndex < m_detentTexts.size() && m_detentTexts[itemIndex] != nullptr)
        {
            controls::Canvas::SetLeft(m_detentTexts[itemIndex], x);
            controls::Canvas::SetTop(m_detentTexts[itemIndex], y);
        }

        if (itemIndex < m_beatTexts.size() && m_beatTexts[itemIndex] != nullptr)
        {
            controls::Canvas::SetLeft(m_beatTexts[itemIndex], x);
            controls::Canvas::SetTop(m_beatTexts[itemIndex], y + m_beatTextOffsets[itemIndex]);
        }

        if (itemIndex < m_tempoTexts.size() && m_tempoTexts[itemIndex] != nullptr)
        {
            controls::Canvas::SetLeft(m_tempoTexts[itemIndex], x);
            controls::Canvas::SetTop(m_tempoTexts[itemIndex], y + m_tempoTextOffsets[itemIndex]);
        }

        if (itemIndex < m_elapsedTexts.size() && m_elapsedTexts[itemIndex] != nullptr)
        {
            controls::Canvas::SetLeft(m_elapsedTexts[itemIndex], x);
            controls::Canvas::SetTop(m_elapsedTexts[itemIndex], y + m_elapsedTextOffsets[itemIndex]);
        }
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

        m_values[itemIndex] = value;

        // Only for a control that is actually showing a number, which is a handful on a page
        // rather than all of them.
        if (itemIndex < m_valueTexts.size() && m_valueTexts[itemIndex] != nullptr)
        {
            RefreshValueText(itemIndex);
        }

        try
        {
            // A switch says what it is by what its plate is painted with. Nothing else on the
            // surface is allowed to fill an area with a hue, which is why "on" reads across a
            // room on a page of two hundred.
            if (visual.IsSwitch && visual.PlateShape != nullptr)
            {
                auto const on = clamped >= 0.5f;

                if (on && visual.PlateOnBrush != nullptr)
                {
                    visual.PlateShape.FillBrush(visual.PlateOnBrush);

                    if (visual.RimOnBrush != nullptr)
                    {
                        visual.PlateShape.StrokeBrush(visual.RimOnBrush);
                    }
                }
                else if (visual.PlateOffBrush != nullptr)
                {
                    visual.PlateShape.FillBrush(visual.PlateOffBrush);

                    if (visual.RimOffBrush != nullptr)
                    {
                        visual.PlateShape.StrokeBrush(visual.RimOffBrush);
                    }
                }

                // The light around it stays up for as long as it is on, rather than decaying
                // the way a single hit does.
                if (visual.Bloom != nullptr && visual.BloomShadow != nullptr)
                {
                    visual.Bloom.StopAnimation(L"Opacity");
                    visual.Bloom.Opacity(on ? SwitchOnGlowOpacity : 0.0f);
                }
            }

            // A platter's value is how far it has been pushed, with the middle meaning not
            // moving, so the marker turns either way from square.
            if (visual.Kind == ControlKind::Turntable)
            {
                if (visual.PointerShape != nullptr)
                {
                    visual.PointerShape.RotationAngleInDegrees(
                        static_cast<float>((clamped - 0.5f) * TurnDegreesAt(itemIndex)));
                }

                return;
            }

            if (visual.ArcGeometry != nullptr)
            {
                visual.ArcGeometry.TrimEnd(clamped * (KnobSweepDegrees / 360.0f));

                if (visual.PointerShape != nullptr)
                {
                    visual.PointerShape.RotationAngleInDegrees(
                        KnobStartAngle + clamped * KnobSweepDegrees);
                }
            }
            else if (visual.PuckGeometry != nullptr)
            {
                MovePuck(itemIndex);
            }
            else if (!visual.RibbonGlow.empty())
            {
                MoveRibbonLight(itemIndex);
            }
            else if (visual.PipeGeometry != nullptr)
            {
                auto const vertical = visual.Vertical;
                auto const filled = visual.TrackLength * clamped;

                auto const fillX = vertical ? visual.PipeCrossOffset : visual.TrackOrigin;
                auto const fillY = vertical
                    ? visual.TrackOrigin + visual.TrackLength - filled
                    : visual.PipeCrossOffset;

                auto const fillW = vertical ? visual.PipeThickness : filled;
                auto const fillH = vertical ? filled : visual.PipeThickness;

                visual.PipeGeometry.Size(float2{ fillW, fillH });
                visual.PipeGeometry.Offset(float2{ fillX, fillY });

                // The cap rides the top of the fill, clamped inside the slot at both ends so it
                // never hangs off the control.
                if (visual.HasThumb && visual.ThumbGeometry != nullptr)
                {
                    auto const half = visual.ThumbLength * 0.5f;

                    auto const travel = std::clamp(
                        vertical
                            ? visual.TrackOrigin + visual.TrackLength - filled
                            : visual.TrackOrigin + filled,
                        visual.TrackOrigin + half,
                        visual.TrackOrigin + visual.TrackLength - half);

                    visual.ThumbGeometry.Offset(vertical
                        ? float2{ visual.ThumbInset, travel - half }
                        : float2{ travel - half, visual.ThumbInset });

                    if (visual.ThumbLineGeometry != nullptr)
                    {
                        auto const lineAcross =
                            visual.ThumbInset + (visual.ThumbSpan - visual.ThumbLineLength) * 0.5f;

                        auto const lineAlong = travel - visual.ThumbLineThickness * 0.5f;

                        visual.ThumbLineGeometry.Offset(vertical
                            ? float2{ lineAcross, lineAlong }
                            : float2{ lineAlong, lineAcross });
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetUnavailable(size_t itemIndex, bool unavailable) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto const& visual = m_visuals[itemIndex];

        if (visual.Unavailable == nullptr || visual.Root == nullptr)
        {
            return;
        }

        try
        {
            visual.Unavailable.IsVisible(unavailable);
            visual.Root.Opacity(unavailable ? UnavailableOpacity : 1.0f);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::Bloom(size_t itemIndex) noexcept
    {
        BloomFor(itemIndex, BloomDecayMilliseconds);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::BloomFeedback(size_t itemIndex) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto const hold = m_visuals[itemIndex].FeedbackHoldMilliseconds;

        BloomFor(itemIndex, hold > 0 ? hold : BloomDecayMilliseconds);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::BloomFor(size_t itemIndex, int64_t milliseconds) noexcept
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
            animation.Duration(std::chrono::milliseconds{ std::max<int64_t>(milliseconds, 1) });

            visual.Bloom.StartAnimation(L"Opacity", animation);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::FlashFeedback(size_t itemIndex) noexcept
    {
        BloomFeedback(itemIndex);

        if (itemIndex >= m_visuals.size() || !m_visuals[itemIndex].IsSwitch)
        {
            return;
        }

        try
        {
            auto const hold = m_visuals[itemIndex].FeedbackHoldMilliseconds;

            SetValue(itemIndex, 1.0);

            m_litUntil[itemIndex] =
                ::GetTickCount64() + static_cast<uint64_t>(hold > 0 ? hold : BloomDecayMilliseconds);

            if (m_flashTimer != nullptr)
            {
                return;
            }

            auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

            if (queue == nullptr)
            {
                return;
            }

            m_flashTimer = queue.CreateTimer();
            m_flashTimer.Interval(std::chrono::milliseconds{ 30 });
            m_flashTimer.Tick([this](auto&&, auto&&) { SweepFlashes(); });
            m_flashTimer.Start();
        }
        catch (...)
        {
        }
    }

    void SurfaceRenderer::SweepFlashes() noexcept
    {
        try
        {
            auto const now = ::GetTickCount64();
            auto anyLit = false;

            for (size_t index = 0; index < m_litUntil.size(); ++index)
            {
                if (m_litUntil[index] == 0)
                {
                    continue;
                }

                if (now >= m_litUntil[index])
                {
                    m_litUntil[index] = 0;
                    SetValue(index, 0.0);
                    continue;
                }

                anyLit = true;
            }

            // Nothing left to turn off, so the page stops paying for a timer until the next
            // thing arrives.
            if (!anyLit && m_flashTimer != nullptr)
            {
                m_flashTimer.Stop();
                m_flashTimer = nullptr;
            }
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

        if (itemIndex < m_litUntil.size())
        {
            m_litUntil[itemIndex] = 0;
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
