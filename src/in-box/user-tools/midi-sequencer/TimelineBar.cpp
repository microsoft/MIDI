// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TimelineBar.h"
#include "TimelineBarHost.h"
#include "ThemeCanvas.h"

#include "StringResources.h"

#include <cmath>

namespace res = ::midisequencer::resources;

namespace midisequencer
{
    namespace
    {
        using winrt::Windows::System::VirtualKey;
        using winrt::Microsoft::UI::Input::InputSystemCursorShape;
        using themeengine::PaintRect;
        using themeengine::PartRect;

        // Win2D takes one family, not CSS's list. Windows 11 has Segoe Fluent Icons.
        constexpr wchar_t GlyphFont[] = L"Segoe Fluent Icons";
        constexpr wchar_t TextFont[] = L"Segoe UI Variable Text";

        constexpr float SegmentWidth = 26.0f;
        constexpr float SegmentHeight = 20.0f;
        constexpr float TogglePadding = 2.0f;
        constexpr float KeyWidth = 26.0f;
        constexpr float KeyHeight = 22.0f;
        constexpr float KeyGap = 6.0f;
        constexpr float SeparatorWidth = 2.0f;
        constexpr float Gap = 10.0f;

        // How far the pointer goes up or down before a drag of the view zooms, and how fast.
        constexpr double VerticalDeadZone = 8.0;
        constexpr double VerticalZoomRate = 0.015;

        constexpr double KeyZoom = 1.25;

        bool Inside(PaintRect const& rect, foundation::Point const& point, float slop = 0.0f) noexcept
        {
            return point.X >= rect.X - slop && point.X < rect.X + rect.Width + slop && point.Y >= rect.Y - slop && point.Y < rect.Y + rect.Height + slop;
        }

        bool ControlDown() noexcept
        {
            return (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        }

        bool ShiftDown() noexcept
        {
            return (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        }

        void SetCursor(xaml::UIElement const& element, InputSystemCursorShape shape)
        {
            // ProtectedCursor is not on the projected type, but the interface carrying it is.
            if (auto const protectedElement = element.try_as<xaml::IUIElementProtected>())
            {
                protectedElement.ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(shape));
            }
        }

        foundation::Rect ToRect(PaintRect const& rect) noexcept
        {
            return foundation::Rect{ rect.X, rect.Y, std::max(0.0f, rect.Width), std::max(0.0f, rect.Height) };
        }
    }

    TimelineBar::~TimelineBar()
    {
        *m_alive = false;

        try
        {
            if (m_host != nullptr)
            {
                auto const host = winrt::get_self<winrt::midisequencer::implementation::TimelineBarHost>(m_host.as<winrt::midisequencer::TimelineBarHost>());
                host->Range = nullptr;
                host->SetValue = nullptr;
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    controls::Grid TimelineBar::Create(TimelineBarActions actions)
    {
        m_actions = std::move(actions);

        auto const host = winrt::make<winrt::midisequencer::implementation::TimelineBarHost>();
        m_host = host.as<controls::Grid>();
        m_host.IsTabStop(true);
        m_host.UseSystemFocusVisuals(true);
        m_host.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });

        automation::AutomationProperties::SetName(m_host, res::GetString(L"TimelineBarName"));
        automation::AutomationProperties::SetHelpText(m_host, res::GetString(L"TimelineBarHelp"));

        std::weak_ptr<bool> const alive = m_alive;

        auto const self = winrt::get_self<winrt::midisequencer::implementation::TimelineBarHost>(host);

        self->Range = [alive, this]()
        {
            winrt::midisequencer::implementation::TimelineBarRange range{};

            if (alive.lock())
            {
                auto const model = CurrentModel();
                auto const bar = static_cast<double>(std::max<int64_t>(1, model.TicksPerBar));
                range.Value = model.View.Start / bar + 1.0;
                range.Minimum = 1.0;
                range.Maximum = std::max(1.0, (model.Extent - model.View.Span) / bar + 1.0);
                range.SmallChange = 1.0;
                range.LargeChange = std::max(1.0, model.View.Span / bar);
            }

            return range;
        };

        self->SetValue = [alive, this](double value)
        {
            if (alive.lock())
            {
                auto const model = CurrentModel();
                auto view = model.View;
                view.Start = (value - 1.0) * static_cast<double>(std::max<int64_t>(1, model.TicksPerBar));
                SetView(ClampView(view, model.Limits));
            }
        };

        m_canvas = canvasXaml::CanvasControl{};
        m_canvas.ClearColor(winrt::Microsoft::UI::Colors::Transparent());
        themeengine::KeepWholePixels(m_canvas);
        m_canvas.ManipulationMode(input::ManipulationModes::Scale);
        automation::AutomationProperties::SetAccessibilityView(m_canvas, automation::Peers::AccessibilityView::Raw);
        m_host.Children().Append(m_canvas);

        m_tip = controls::ToolTip{};

        m_canvas.Draw([alive, this](canvasXaml::CanvasControl const& sender, canvasXaml::CanvasDrawEventArgs const& args)
        {
            if (alive.lock())
            {
                try
                {
                    m_steady.Draw(sender, args, [&](canvas::CanvasDrawingSession const& ds)
                    {
                        Draw(ds, sender, static_cast<float>(sender.ActualWidth()), static_cast<float>(sender.ActualHeight()));
                    });
                }
                MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw the timeline's scroll bar.")
            }
        });

        m_canvas.CreateResources([alive, this](auto&&, auto&&)
        {
            if (alive.lock())
            {
                m_painter.Reset();
            }
        });

        m_canvas.PointerPressed([alive, this](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnPressed(args); }
        });

        m_canvas.PointerMoved([alive, this](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnMoved(args); }
        });

        m_canvas.PointerReleased([alive, this](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnReleased(args, false); }
        });

        m_canvas.PointerCaptureLost([alive, this](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnReleased(args, true); }
        });

        m_canvas.PointerExited([alive, this](auto&&, auto&&)
        {
            try
            {
                if (alive.lock() && !m_dragging) { SetHover(Hit::None); }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the timeline's scroll bar.")
        });

        m_canvas.PointerWheelChanged([alive, this](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnWheel(args); }
        });

        m_canvas.DoubleTapped([alive, this](auto&&, input::DoubleTappedRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnDoubleTapped(args); }
        });

        m_canvas.RightTapped([alive, this](auto&&, input::RightTappedRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnRightTapped(args); }
        });

        m_canvas.ManipulationDelta([alive, this](auto&&, input::ManipulationDeltaRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnPinch(args); }
        });

        m_host.KeyDown([alive, this](auto&&, input::KeyRoutedEventArgs const& args)
        {
            if (alive.lock()) { OnKeyDown(args); }
        });

        return m_host;
    }

    _Use_decl_annotations_
    void TimelineBar::Palette(midisequencer::Palette const* palette) noexcept
    {
        m_palette = palette;
        m_sectionFormat = nullptr;
        m_glyphFormat = nullptr;
        Invalidate();
    }

    _Use_decl_annotations_
    void TimelineBar::Overview(bool value) noexcept
    {
        m_overview = value;
        Invalidate();
    }

    _Use_decl_annotations_
    void TimelineBar::AtTop(bool value) noexcept
    {
        m_atTop = value;
        Invalidate();
    }

    std::shared_ptr<themeengine::ResolvedTheme const> TimelineBar::Theme() const noexcept
    {
        return m_palette != nullptr ? m_palette->Theme : nullptr;
    }

    _Use_decl_annotations_
    float TimelineBar::Metric(std::wstring_view name, float fallback) const
    {
        auto const theme = Theme();
        return theme != nullptr ? static_cast<float>(theme->Metric(name, fallback)) : fallback;
    }

    double TimelineBar::RowHeight() const noexcept
    {
        try
        {
            return m_overview ? Metric(L"timelineOverviewHeight", 42.0f) : Metric(L"timelinePlainHeight", 22.0f);
        }
        catch (...)
        {
            return m_overview ? 42.0 : 22.0;
        }
    }

    void TimelineBar::Invalidate() noexcept
    {
        try
        {
            if (m_canvas != nullptr)
            {
                m_canvas.Invalidate();
            }

            if (m_host != nullptr && m_actions.Model)
            {
                UpdateName(CurrentModel());
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void TimelineBar::PlayheadMoved(int64_t tick) noexcept
    {
        try
        {
            if (m_canvas == nullptr || !m_actions.Model)
            {
                return;
            }

            auto const model = CurrentModel();
            auto const layout = CurrentLayout(model);
            auto const x = std::round(static_cast<float>(layout.Scale.XAt(static_cast<double>(tick))));

            if (x != m_playheadX)
            {
                m_canvas.Invalidate();
            }
        }
        catch (...)
        {
        }
    }

    void TimelineBar::ToggleFit() noexcept
    {
        try
        {
            if (m_beforeFit.has_value())
            {
                auto const back = *m_beforeFit;
                m_beforeFit.reset();
                SetView(back);
                return;
            }

            m_beforeFit = CurrentModel().View;

            if (m_actions.ZoomToFit)
            {
                m_actions.ZoomToFit();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to zoom the timeline to fit.")
    }

    TimelineBarModel TimelineBar::CurrentModel() const
    {
        return m_actions.Model ? m_actions.Model() : TimelineBarModel{};
    }

    _Use_decl_annotations_
    TimelineBar::Layout TimelineBar::CurrentLayout(TimelineBarModel const& model) const
    {
        auto const width = m_canvas != nullptr ? static_cast<float>(m_canvas.ActualWidth()) : 0.0f;
        auto const height = m_canvas != nullptr ? static_cast<float>(m_canvas.ActualHeight()) : 0.0f;
        return Arrange(model, width, height);
    }

    _Use_decl_annotations_
    void TimelineBar::SetView(TimelineSpan const& view)
    {
        if (m_actions.SetView)
        {
            m_actions.SetView(view);
        }
    }

    _Use_decl_annotations_
    void TimelineBar::UpdateName(TimelineBarModel const& model)
    {
        auto const bar = static_cast<double>(std::max<int64_t>(1, model.TicksPerBar));
        auto const first = static_cast<int64_t>(std::floor(model.View.Start / bar)) + 1;
        auto const last = std::max(first, static_cast<int64_t>(std::ceil(model.View.End() / bar)));
        auto const total = std::max(last, static_cast<int64_t>(std::ceil(model.Extent / bar)));

        std::wstring text{ res::FormatString(L"TimelineBarViewFormat", first, last, total) };

        if (text != m_name)
        {
            m_name = std::move(text);
            automation::AutomationProperties::SetItemStatus(m_host, winrt::hstring{ m_name });
        }
    }

    // ---------------------------------------------------------------- where things are

    _Use_decl_annotations_
    TimelineBar::Layout TimelineBar::Arrange(TimelineBarModel const& model, float width, float height) const
    {
        Layout layout{};

        auto const insetLeft = Metric(L"timelineBarLeft", 6.0f);
        auto const insetRight = Metric(L"timelineBarRight", 8.0f);
        auto const middle = height / 2.0f;

        auto const laneLeft = model.LaneLeft + insetLeft;
        auto const laneRight = std::max(laneLeft + 40.0f, std::min(width, model.LaneLeft + model.LaneWidth) - insetRight);

        auto const toggleWidth = SegmentWidth * 2.0f + TogglePadding * 3.0f;
        auto const toggleHeight = SegmentHeight + TogglePadding * 2.0f;
        auto const keysWidth = KeyWidth * 3.0f + KeyGap * 3.0f + SeparatorWidth;

        // Under the timeline, all of it is in the timeline's column; above the ruler, the switch
        // and the keys are over the track list, at its right.
        float toolsRight{};
        float trackLeft{};
        float trackRight{};

        if (m_atTop)
        {
            toolsRight = std::max(toggleWidth + keysWidth + Gap, model.HeaderWidth - 10.0f);
            trackLeft = laneLeft;
            trackRight = laneRight;
        }
        else
        {
            toolsRight = laneRight;
            trackLeft = laneLeft + toggleWidth + Gap;
            trackRight = laneRight - keysWidth - Gap;
        }

        auto const toggleLeft = m_atTop ? toolsRight - keysWidth - Gap - toggleWidth : laneLeft;
        layout.Toggle = { toggleLeft, middle - toggleHeight / 2.0f, toggleWidth, toggleHeight };
        layout.Segments[0] = { toggleLeft + TogglePadding, middle - SegmentHeight / 2.0f, SegmentWidth, SegmentHeight };
        layout.Segments[1] = { toggleLeft + TogglePadding * 2.0f + SegmentWidth, middle - SegmentHeight / 2.0f, SegmentWidth, SegmentHeight };

        // zoom out, zoom in, a separator, zoom to fit
        auto const keysLeft = toolsRight - keysWidth;
        layout.Keys[0] = { keysLeft, middle - KeyHeight / 2.0f, KeyWidth, KeyHeight };
        layout.Keys[1] = { keysLeft + KeyWidth + KeyGap, middle - KeyHeight / 2.0f, KeyWidth, KeyHeight };
        layout.Separator = { keysLeft + KeyWidth * 2.0f + KeyGap * 2.0f, middle - 8.0f, SeparatorWidth, 16.0f };
        layout.Keys[2] = { layout.Separator.X + SeparatorWidth + KeyGap, middle - KeyHeight / 2.0f, KeyWidth, KeyHeight };

        auto const trackHeight = m_overview ? Metric(L"timelineOverviewTrack", 30.0f) : Metric(L"timelinePlainTrack", 6.0f);
        layout.Track = { trackLeft, middle - trackHeight / 2.0f, std::max(1.0f, trackRight - trackLeft), trackHeight };

        layout.Scale = m_frozenScale.value_or(TimelineBarScale{ layout.Track.X, layout.Track.Width, std::max(1.0, model.Extent) });
        layout.ViewLeft = static_cast<float>(layout.Scale.XAt(model.View.Start));
        layout.ViewRight = static_cast<float>(layout.Scale.XAt(model.View.End()));

        // A view too narrow to find keeps a few pixels.
        if (layout.ViewRight - layout.ViewLeft < 6.0f)
        {
            auto const center = (layout.ViewLeft + layout.ViewRight) / 2.0f;
            layout.ViewLeft = center - 3.0f;
            layout.ViewRight = center + 3.0f;
        }

        return layout;
    }

    _Use_decl_annotations_
    TimelineBar::Hit TimelineBar::HitTest(Layout const& layout, foundation::Point const& point) const noexcept
    {
        if (Inside(layout.Segments[0], point, 1.0f))
        {
            return Hit::Plain;
        }

        if (Inside(layout.Segments[1], point, 1.0f))
        {
            return Hit::Overview;
        }

        Hit const keys[]{ Hit::ZoomOut, Hit::ZoomIn, Hit::ZoomFit };

        for (size_t i = 0; i < 3; ++i)
        {
            if (Inside(layout.Keys[i], point, 1.0f))
            {
                return keys[i];
            }
        }

        // The track takes the whole height of the row, so it's easy to hit.
        if (point.X < layout.Track.X - 4.0f || point.X > layout.Track.X + layout.Track.Width + 4.0f)
        {
            return Hit::None;
        }

        auto const width = layout.ViewRight - layout.ViewLeft;
        auto const ends = std::min(8.0f, std::max(3.0f, width / 3.0f));

        if (std::abs(point.X - layout.ViewLeft) <= ends)
        {
            return Hit::LeftEnd;
        }

        if (std::abs(point.X - layout.ViewRight) <= ends)
        {
            return Hit::RightEnd;
        }

        if (point.X > layout.ViewLeft && point.X < layout.ViewRight)
        {
            return Hit::View;
        }

        return Hit::Track;
    }

    // ---------------------------------------------------------------- drawing

    _Use_decl_annotations_
    bool TimelineBar::PaintPart(
        canvas::CanvasDrawingSession const& ds,
        canvas::ICanvasResourceCreatorWithDpi const& creator,
        std::wstring_view part,
        std::vector<std::wstring_view> const& states,
        PaintRect const& rect)
    {
        auto const theme = Theme();

        if (theme == nullptr || !theme->HasPart(part))
        {
            return false;
        }

        auto const style = theme->Part(part, std::span<std::wstring_view const>{ states });
        m_painter.Paint(ds, creator, *style, PartRect(*style, rect));
        return true;
    }

    _Use_decl_annotations_
    void TimelineBar::Draw(canvas::CanvasDrawingSession const& ds, canvas::ICanvasResourceCreatorWithDpi const& creator, float width, float height)
    {
        if (m_palette == nullptr || width <= 0 || height <= 0)
        {
            return;
        }

        auto const& colors = *m_palette;
        auto const theme = Theme();

        if (m_glyphFormat == nullptr)
        {
            m_glyphFormat = canvasText::CanvasTextFormat{};
            m_glyphFormat.FontFamily(GlyphFont);
            m_glyphFormat.FontSize(11.0f);
            m_glyphFormat.HorizontalAlignment(canvasText::CanvasHorizontalAlignment::Center);
            m_glyphFormat.VerticalAlignment(canvasText::CanvasVerticalAlignment::Center);
        }

        if (m_sectionFormat == nullptr)
        {
            m_sectionFont = theme != nullptr ? theme->Font(L"label", TextFont) : std::wstring{ TextFont };
            m_sectionFormat = canvasText::CanvasTextFormat{};
            m_sectionFormat.FontFamily(winrt::hstring{ m_sectionFont });
            m_sectionFormat.FontSize(10.0f);
            m_sectionFormat.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);
            m_sectionFormat.TrimmingGranularity(canvasText::CanvasTextTrimmingGranularity::Character);
            m_sectionFormat.VerticalAlignment(canvasText::CanvasVerticalAlignment::Top);
        }

        auto const model = CurrentModel();
        auto const layout = Arrange(model, width, height);

        // A line between the bar and the timeline, where the theme has dividers.
        if (colors.Divider.A > 0)
        {
            ds.FillRectangle(0, m_atTop ? height - 1.0f : 0.0f, width, 1.0f, colors.Divider);
        }

        DrawToggle(ds, creator, layout);
        DrawKeys(ds, creator, layout);

        if (m_overview)
        {
            DrawOverview(ds, creator, layout, model);
        }
        else
        {
            DrawPlain(ds, creator, layout, model);
        }

        m_playheadX = model.Playhead >= 0 ? std::round(static_cast<float>(layout.Scale.XAt(static_cast<double>(model.Playhead)))) : -1.0f;
    }

    _Use_decl_annotations_
    void TimelineBar::DrawToggle(canvas::CanvasDrawingSession const& ds, canvas::ICanvasResourceCreatorWithDpi const& creator, Layout const& layout)
    {
        auto const& colors = *m_palette;
        auto const themed = PaintPart(ds, creator, L"timeline.bar.toggle", {}, layout.Toggle);

        if (!themed)
        {
            ds.DrawRoundedRectangle(ToRect(layout.Toggle), 5.0f, 5.0f, WithAlpha(colors.Text, colors.Light ? 0.16 : 0.18), 1.0f);
        }

        for (size_t i = 0; i < 2; ++i)
        {
            auto const on = (i == 1) == m_overview;
            auto const hover = m_hover == (i == 1 ? Hit::Overview : Hit::Plain);
            auto const& rect = layout.Segments[i];

            std::vector<std::wstring_view> states{};

            if (hover)
            {
                states.emplace_back(L"hover");
            }

            if (on)
            {
                states.emplace_back(L"checked");
            }

            Color ink = on ? colors.Text : colors.Text3;

            if (themed && Theme()->HasPart(L"timeline.bar.segment"))
            {
                PaintPart(ds, creator, L"timeline.bar.segment", states, rect);
                auto const style = Theme()->Part(L"timeline.bar.segment", std::span<std::wstring_view const>{ states });

                if (style->Foreground.has_value())
                {
                    ink = ToUiColor(*style->Foreground);
                }
            }
            else if (on || hover)
            {
                ds.FillRoundedRectangle(ToRect(rect), 4.0f, 4.0f, WithAlpha(colors.Text, on ? 0.13 : 0.06));
            }

            auto const cx = rect.X + rect.Width / 2.0f;
            auto const cy = rect.Y + rect.Height / 2.0f;

            if (i == 0)
            {
                // the plain bar: a slot with a thumb in it
                ds.DrawRoundedRectangle(cx - 6.5f, cy - 3.0f, 13.0f, 6.0f, 3.0f, 3.0f, ink, 1.2f);
                ds.FillRoundedRectangle(cx - 3.0f, cy - 1.5f, 5.5f, 3.0f, 1.5f, 1.5f, ink);
            }
            else
            {
                // the overview: three tracks' clips, in color when it's on
                ds.FillRoundedRectangle(cx - 7.0f, cy - 4.4f, 8.0f, 1.8f, 0.9f, 0.9f, on ? colors.Record : ink);
                ds.FillRoundedRectangle(cx - 3.0f, cy - 0.9f, 9.5f, 1.8f, 0.9f, 0.9f, on ? colors.Accent : ink);
                ds.FillRoundedRectangle(cx - 5.5f, cy + 2.6f, 6.0f, 1.8f, 0.9f, 0.9f, on ? colors.Play : ink);
            }
        }
    }

    _Use_decl_annotations_
    void TimelineBar::DrawKeys(canvas::CanvasDrawingSession const& ds, canvas::ICanvasResourceCreatorWithDpi const& creator, Layout const& layout)
    {
        auto const& colors = *m_palette;
        auto const theme = Theme();

        static constexpr std::wstring_view glyphs[]{ L"\uE71F", L"\uE8A3", L"\uE9A6" };
        Hit const keys[]{ Hit::ZoomOut, Hit::ZoomIn, Hit::ZoomFit };

        for (size_t i = 0; i < 3; ++i)
        {
            auto const hover = m_hover == keys[i];
            auto const pressed = m_pressed == keys[i] && hover;
            auto const& rect = layout.Keys[i];

            std::vector<std::wstring_view> states{};

            if (hover)
            {
                states.emplace_back(L"hover");
            }

            if (pressed)
            {
                states.emplace_back(L"pressed");
            }

            Color ink = colors.Text2;

            if (PaintPart(ds, creator, L"timeline.bar.key", states, rect))
            {
                auto const style = theme->Part(L"timeline.bar.key", std::span<std::wstring_view const>{ states });

                if (style->Foreground.has_value())
                {
                    ink = ToUiColor(*style->Foreground);
                }
            }
            else if (hover)
            {
                ds.FillRoundedRectangle(ToRect(rect), 4.0f, 4.0f, WithAlpha(colors.Text, pressed ? 0.14 : 0.08));
            }

            ds.DrawText(winrt::hstring{ glyphs[i] }, ToRect(rect), ink, m_glyphFormat);
        }

        if (!PaintPart(ds, creator, L"timeline.bar.separator", {}, layout.Separator))
        {
            ds.FillRectangle(std::floor(layout.Separator.X), layout.Separator.Y, 1.0f, layout.Separator.Height, WithAlpha(colors.Text, 0.16));
        }
    }

    _Use_decl_annotations_
    void TimelineBar::DrawOverview(
        canvas::CanvasDrawingSession const& ds,
        canvas::ICanvasResourceCreatorWithDpi const& creator,
        Layout const& layout,
        TimelineBarModel const& model)
    {
        auto const& colors = *m_palette;
        auto const theme = Theme();
        auto const& track = layout.Track;
        auto const& scale = layout.Scale;

        // The well, and the colors drawn in it.
        std::shared_ptr<themeengine::ResolvedStyle const> look{};
        auto const themed = theme != nullptr && theme->HasPart(L"timeline.bar.well");

        if (theme != nullptr && theme->HasPart(L"timeline.bar.overview"))
        {
            look = theme->Part(L"timeline.bar.overview");
        }

        auto const color = [&look](std::wstring_view name, Color fallback)
        {
            return look != nullptr ? ToUiColor(look->ColorOr(name, ToThemeColor(fallback))) : fallback;
        };

        auto const bandColor = color(L"band-color", colors.Light ? Rgba(0, 0, 0, 0.035) : Rgba(255, 255, 255, 0.03));
        auto const sectionColor = color(L"section-color", colors.Text2);
        auto const loopColor = color(L"loop-color", WithAlpha(colors.Accent, 0.22));
        auto const foundColor = color(L"found-color", colors.Accent);
        auto const playheadColor = color(L"playhead-color", colors.Record);
        auto const veilColor = color(L"veil-color", colors.Light ? Rgba(255, 255, 255, 0.5) : Rgba(0, 0, 0, 0.42));
        auto const lineBrightness = look != nullptr ? look->Number(L"line-brightness", 1.0) : 1.0;

        float radius = 5.0f;

        if (themed)
        {
            auto const well = theme->Part(L"timeline.bar.well");
            m_painter.Paint(ds, creator, *well, PartRect(*well, track));
            radius = static_cast<float>(well->CornerRadius(0, PartRect(*well, track).Width, PartRect(*well, track).Height));
        }
        else
        {
            ds.FillRoundedRectangle(ToRect(track), radius, radius, colors.Light ? Rgba(0, 0, 0, 0.05) : Rgba(0, 0, 0, 0.24));
            ds.DrawRoundedRectangle(track.X + 0.5f, track.Y + 0.5f, track.Width - 1.0f, track.Height - 1.0f, radius, radius, WithAlpha(colors.Text, 0.10), 1.0f);
        }

        auto const x = [&scale](double tick) { return static_cast<float>(scale.XAt(tick)); };
        auto const shape = canvasGeometry::CanvasGeometry::CreateRoundedRectangle(creator, ToRect(track), radius, radius);

        {
            auto layer = ds.CreateLayer(1.0f, shape);

            auto const top = track.Y + 13.0f;
            auto const bottom = track.Y + track.Height - 5.0f;

            // the sections: a band each, every other one deeper, a mark where each starts, and its name
            for (size_t i = 0; i < model.Sections.size(); ++i)
            {
                auto const& section = model.Sections[i];
                auto const until = i + 1 < model.Sections.size() ? static_cast<double>(model.Sections[i + 1].Tick) : model.Extent;
                auto const x0 = x(static_cast<double>(section.Tick));
                auto const x1 = x(until);

                if (i % 2 == 1)
                {
                    ds.FillRectangle(x0, track.Y, std::max(0.0f, x1 - x0), track.Height, bandColor);
                }

                ds.FillRectangle(x0, track.Y, 1.5f, track.Height, Mix(section.Color.value_or(colors.Text3), 0.55, colors.Text));

                if (x1 - x0 > 14.0f && !section.Name.empty())
                {
                    ds.DrawText(winrt::hstring{ section.Name }, foundation::Rect{ x0 + 5.0f, track.Y + 1.0f, x1 - x0 - 8.0f, 12.0f }, sectionColor, m_sectionFormat);
                }
            }

            if (model.LoopEnd > model.LoopStart)
            {
                auto const x0 = x(static_cast<double>(model.LoopStart));
                auto const x1 = x(static_cast<double>(model.LoopEnd));
                ds.FillRoundedRectangle(x0, top - 2.0f, std::max(1.0f, x1 - x0), bottom - top + 4.0f, 2.0f, 2.0f, model.LoopOn ? loopColor : WithAlpha(loopColor, loopColor.A / 255.0 * 0.4));
            }

            // A line for each track where its clips are. With more tracks than room, they share it.
            if (!model.Lines.empty())
            {
                auto const count = static_cast<float>(model.Lines.size());
                auto const step = (bottom - top) / count;
                auto const gap = step >= 3.0f ? std::min(1.2f, step / 4.0f) : 0.0f;
                auto const lineHeight = std::max(1.0f, step - gap);

                for (size_t i = 0; i < model.Lines.size(); ++i)
                {
                    auto const& line = model.Lines[i];
                    auto const y = top + static_cast<float>(i) * step;
                    auto ink = lineBrightness != 1.0 ? Desaturate(line.Color, 1.0, lineBrightness) : line.Color;

                    if (line.Muted)
                    {
                        ink = WithAlpha(ink, 0.3);
                    }

                    for (auto const& [from, to] : line.Spans)
                    {
                        auto const x0 = x(static_cast<double>(from)) + 0.5f;
                        auto const x1 = x(static_cast<double>(to)) - 0.5f;
                        ds.FillRectangle(x0, y, std::max(1.0f, x1 - x0), lineHeight, ink);
                    }
                }
            }

            // under the lines, the selected clip and where else it plays
            for (size_t i = 0; i < model.Linked.size(); ++i)
            {
                auto const x0 = x(static_cast<double>(model.Linked[i].first)) + 0.5f;
                auto const x1 = x(static_cast<double>(model.Linked[i].second)) - 0.5f;
                ds.FillRoundedRectangle(x0, bottom + 1.5f, std::max(2.0f, x1 - x0), 2.5f, 1.25f, 1.25f, i == 0 ? foundColor : WithAlpha(foundColor, 0.55));
            }

            if (model.Playhead >= 0)
            {
                ds.FillRectangle(x(static_cast<double>(model.Playhead)) - 0.75f, track.Y, 1.5f, track.Height, playheadColor);
            }

            // what's outside the view, veiled
            ds.FillRectangle(track.X, track.Y, std::max(0.0f, layout.ViewLeft - track.X), track.Height, veilColor);
            ds.FillRectangle(layout.ViewRight, track.Y, std::max(0.0f, track.X + track.Width - layout.ViewRight), track.Height, veilColor);

            layer.Close();
        }

        // the view: a frame, with a grip at each end to zoom
        PaintRect const view{ layout.ViewLeft, track.Y, layout.ViewRight - layout.ViewLeft, track.Height };
        auto const dragging = m_dragging && (m_pressed == Hit::View || m_pressed == Hit::LeftEnd || m_pressed == Hit::RightEnd);

        std::vector<std::wstring_view> viewStates{};

        if (m_hover == Hit::View || m_hover == Hit::LeftEnd || m_hover == Hit::RightEnd)
        {
            viewStates.emplace_back(L"hover");
        }

        if (dragging)
        {
            viewStates.emplace_back(L"pressed");
        }

        if (!PaintPart(ds, creator, L"timeline.bar.view", viewStates, view))
        {
            ds.DrawRoundedRectangle(view.X + 0.75f, view.Y + 0.75f, std::max(1.0f, view.Width - 1.5f), view.Height - 1.5f, 4.0f, 4.0f,
                WithAlpha(colors.Text, viewStates.empty() ? 0.7 : 0.95), 1.5f);
        }

        for (auto const right : { false, true })
        {
            auto const end = right ? Hit::RightEnd : Hit::LeftEnd;
            auto const hot = m_hover == end || (m_dragging && m_pressed == end);
            auto const at = right ? layout.ViewRight : layout.ViewLeft;

            std::vector<std::wstring_view> states{};

            if (hot)
            {
                states.emplace_back(L"hover");
            }

            PaintRect const space{ right ? at - 6.0f : at - 3.0f, track.Y, 9.0f, track.Height };

            if (!PaintPart(ds, creator, L"timeline.bar.grip", states, space))
            {
                ds.FillRoundedRectangle(at - 2.0f, track.Y + track.Height / 2.0f - 8.0f, 4.0f, 16.0f, 2.0f, 2.0f, hot ? colors.Accent : WithAlpha(colors.Text, 0.85));
            }
        }
    }

    _Use_decl_annotations_
    void TimelineBar::DrawPlain(
        canvas::CanvasDrawingSession const& ds,
        canvas::ICanvasResourceCreatorWithDpi const& creator,
        Layout const& layout,
        TimelineBarModel const& model)
    {
        auto const& colors = *m_palette;
        auto const theme = Theme();
        auto const& slot = layout.Track;

        if (!PaintPart(ds, creator, L"timeline.bar.slot", {}, slot))
        {
            ds.FillRoundedRectangle(ToRect(slot), slot.Height / 2.0f, slot.Height / 2.0f, WithAlpha(colors.Text, colors.Light ? 0.08 : 0.10));
        }

        auto const thumbHeight = Metric(L"timelineThumbHeight", 12.0f);
        auto const middle = slot.Y + slot.Height / 2.0f;
        PaintRect const thumb{ layout.ViewLeft, middle - thumbHeight / 2.0f, layout.ViewRight - layout.ViewLeft, thumbHeight };

        auto const hover = m_hover == Hit::View || m_hover == Hit::LeftEnd || m_hover == Hit::RightEnd;
        auto const dragging = m_dragging && (m_pressed == Hit::View || m_pressed == Hit::LeftEnd || m_pressed == Hit::RightEnd);

        std::vector<std::wstring_view> states{};

        if (hover)
        {
            states.emplace_back(L"hover");
        }

        if (dragging)
        {
            states.emplace_back(L"pressed");
        }

        Color gripDark = WithAlpha(colors.Text, 0.55);
        Color gripLight{};

        if (PaintPart(ds, creator, L"timeline.bar.thumb", states, thumb))
        {
            auto const style = theme->Part(L"timeline.bar.thumb", std::span<std::wstring_view const>{ states });
            gripDark = ToUiColor(style->ColorOr(L"grip-color", ToThemeColor(gripDark)));
            gripLight = ToUiColor(style->ColorOr(L"grip-light-color", ToThemeColor(gripLight)));
        }
        else
        {
            ds.FillRoundedRectangle(ToRect(thumb), thumbHeight / 2.0f, thumbHeight / 2.0f, WithAlpha(colors.Text, dragging ? 0.5 : hover ? 0.42 : 0.3));
        }

        // two engraved lines at each end, lit when that end is the one to drag
        if (thumb.Width > 24.0f)
        {
            for (auto const right : { false, true })
            {
                auto const end = right ? Hit::RightEnd : Hit::LeftEnd;
                auto const hot = m_hover == end || (m_dragging && m_pressed == end);
                auto const left = right ? thumb.X + thumb.Width - 11.0f : thumb.X + 6.0f;

                for (auto const offset : { 0.0f, 3.0f })
                {
                    ds.FillRectangle(left + offset, middle - thumbHeight / 2.0f + 3.0f, 1.0f, thumbHeight - 6.0f, hot ? colors.Accent : gripDark);

                    if (gripLight.A > 0 && !hot)
                    {
                        ds.FillRectangle(left + offset + 1.0f, middle - thumbHeight / 2.0f + 3.0f, 1.0f, thumbHeight - 6.0f, gripLight);
                    }
                }
            }
        }

        if (model.Playhead >= 0)
        {
            auto const x = static_cast<float>(layout.Scale.XAt(static_cast<double>(model.Playhead)));
            ds.FillRoundedRectangle(x - 1.0f, middle - thumbHeight / 2.0f - 1.0f, 2.0f, thumbHeight + 2.0f, 1.0f, 1.0f, colors.Record);
        }
    }

    // ---------------------------------------------------------------- input

    _Use_decl_annotations_
    void TimelineBar::SetHover(Hit hit)
    {
        if (hit == m_hover)
        {
            return;
        }

        m_hover = hit;

        SetCursor(m_canvas, hit == Hit::LeftEnd || hit == Hit::RightEnd ? InputSystemCursorShape::SizeWestEast : InputSystemCursorShape::Arrow);

        std::wstring_view key{};

        switch (hit)
        {
        case Hit::Plain: key = L"TimelineBarPlainTip"; break;
        case Hit::Overview: key = L"TimelineBarOverviewTip"; break;
        case Hit::ZoomOut: key = L"TimelineZoomOutTip"; break;
        case Hit::ZoomIn: key = L"TimelineZoomInTip"; break;
        case Hit::ZoomFit: key = L"TimelineZoomFitTip"; break;
        case Hit::LeftEnd:
        case Hit::RightEnd: key = L"TimelineBarEndTip"; break;
        case Hit::View: key = L"TimelineBarViewTip"; break;
        default: break;
        }

        if (key.empty())
        {
            m_tip.IsOpen(false);
            controls::ToolTipService::SetToolTip(m_host, nullptr);
        }
        else
        {
            m_tip.Content(winrt::box_value(res::GetString(key)));
            controls::ToolTipService::SetToolTip(m_host, m_tip);
        }

        m_canvas.Invalidate();
    }

    _Use_decl_annotations_
    void TimelineBar::OnPressed(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const point = args.GetCurrentPoint(m_canvas);

            if (point.Properties().IsRightButtonPressed() || point.Properties().IsMiddleButtonPressed())
            {
                return;
            }

            auto const model = CurrentModel();
            auto const layout = Arrange(model, static_cast<float>(m_canvas.ActualWidth()), static_cast<float>(m_canvas.ActualHeight()));
            auto const position = point.Position();
            auto hit = HitTest(layout, position);

            m_host.Focus(xaml::FocusState::Pointer);

            if (hit == Hit::None)
            {
                return;
            }

            args.Handled(true);

            if (hit == Hit::Plain || hit == Hit::Overview)
            {
                if (m_actions.SetOverview)
                {
                    m_actions.SetOverview(hit == Hit::Overview);
                }

                return;
            }

            m_pressed = hit;
            m_pressPoint = position;
            m_pointerId = args.Pointer().PointerId();
            m_canvas.CapturePointer(args.Pointer());

            if (hit == Hit::ZoomOut || hit == Hit::ZoomIn || hit == Hit::ZoomFit)
            {
                m_canvas.Invalidate();
                return;
            }

            // A press alone doesn't forget the view to go back to: a double-click is two presses.
            m_frozenScale = layout.Scale;
            m_pressLimits = model.Limits;
            m_pressView = model.View;

            auto const tick = layout.Scale.TickAt(position.X);

            // Pressed beside the view: it jumps there, centered, and follows the pointer.
            if (hit == Hit::Track)
            {
                m_pressView = ClampView(TimelineSpan{ tick - model.View.Span / 2.0, model.View.Span }, model.Limits);
                ForgetFit();
                SetView(m_pressView);
                hit = Hit::View;
                m_pressed = hit;
            }

            m_pressOffset = hit == Hit::LeftEnd ? m_pressView.Start - tick
                : hit == Hit::RightEnd ? m_pressView.End() - tick
                : tick - m_pressView.Start;

            m_dragging = true;
            m_canvas.Invalidate();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start a drag on the timeline's scroll bar.")
    }

    _Use_decl_annotations_
    void TimelineBar::OnMoved(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const position = args.GetCurrentPoint(m_canvas).Position();

            if (!m_dragging)
            {
                if (m_pressed == Hit::None)
                {
                    SetHover(HitTest(CurrentLayout(CurrentModel()), position));
                }
                else
                {
                    // A key held: lit while the pointer is on it.
                    auto const over = HitTest(CurrentLayout(CurrentModel()), position);
                    SetHover(over == m_pressed ? over : Hit::None);
                }

                return;
            }

            auto const scale = m_frozenScale.value_or(TimelineBarScale{});
            auto const tick = scale.TickAt(position.X);

            // Moved, not just pressed: double-clicking fits again rather than going back.
            if (std::abs(position.X - m_pressPoint.X) + std::abs(position.Y - m_pressPoint.Y) < 2.0f)
            {
                return;
            }

            ForgetFit();

            if (m_pressed == Hit::LeftEnd || m_pressed == Hit::RightEnd)
            {
                SetView(DragViewEnd(m_pressView, m_pressed == Hit::RightEnd, tick + m_pressOffset, m_pressLimits));
            }
            else if (m_pressed == Hit::View)
            {
                // Up zooms in and down zooms out, past a little give, about the tick under the
                // pointer.
                auto const dy = static_cast<double>(position.Y - m_pressPoint.Y);
                auto const beyond = std::abs(dy) > VerticalDeadZone ? dy - std::copysign(VerticalDeadZone, dy) : 0.0;
                auto const most = std::max(m_pressLimits.MinSpan, m_pressLimits.MaxSpan);
                auto const span = std::clamp(m_pressView.Span * std::exp(beyond * VerticalZoomRate), m_pressLimits.MinSpan, most);
                auto const share = m_pressOffset / m_pressView.Span;

                SetView(ClampView(TimelineSpan{ tick - share * span, span }, m_pressLimits));
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to drag on the timeline's scroll bar.")
    }

    _Use_decl_annotations_
    void TimelineBar::OnReleased(input::PointerRoutedEventArgs const& args, bool lost)
    {
        try
        {
            auto const pressed = m_pressed;
            auto const wasDragging = m_dragging;

            m_pressed = Hit::None;
            m_dragging = false;
            m_frozenScale.reset();

            if (!lost && pressed != Hit::None)
            {
                m_canvas.ReleasePointerCapture(args.Pointer());
            }

            if (!lost && !wasDragging && (pressed == Hit::ZoomOut || pressed == Hit::ZoomIn || pressed == Hit::ZoomFit))
            {
                auto const over = HitTest(CurrentLayout(CurrentModel()), args.GetCurrentPoint(m_canvas).Position());

                if (over == pressed)
                {
                    ForgetFit();

                    if (pressed == Hit::ZoomFit)
                    {
                        if (m_actions.ZoomToFit) { m_actions.ZoomToFit(); }
                    }
                    else if (m_actions.ZoomStep)
                    {
                        m_actions.ZoomStep(pressed == Hit::ZoomIn ? KeyZoom : 1.0 / KeyZoom);
                    }
                }
            }

            if (pressed != Hit::None)
            {
                args.Handled(true);
            }

            m_canvas.Invalidate();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish a drag on the timeline's scroll bar.")
    }

    _Use_decl_annotations_
    void TimelineBar::OnWheel(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            using winrt::Windows::System::VirtualKeyModifiers;

            auto const point = args.GetCurrentPoint(m_canvas);
            auto const delta = static_cast<double>(point.Properties().MouseWheelDelta());
            auto const control = (args.KeyModifiers() & VirtualKeyModifiers::Control) == VirtualKeyModifiers::Control;
            auto const model = CurrentModel();
            auto const layout = CurrentLayout(model);

            ForgetFit();

            if (control)
            {
                // A touchpad's pinch comes as Ctrl and the wheel, in small steps.
                auto const anchor = std::clamp(layout.Scale.TickAt(point.Position().X), 0.0, model.Extent);
                SetView(ZoomAbout(model.View, std::pow(KeyZoom, delta / 120.0), anchor, model.Limits));
            }
            else
            {
                // Down or right moves later in the sequence.
                auto const direction = point.Properties().IsHorizontalMouseWheel() ? 1.0 : -1.0;
                auto view = model.View;
                view.Start += direction * delta / 120.0 * view.Span * 0.15;
                SetView(ClampView(view, model.Limits));
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to scroll the timeline.")
    }

    _Use_decl_annotations_
    void TimelineBar::OnPinch(input::ManipulationDeltaRoutedEventArgs const& args)
    {
        try
        {
            auto const scale = static_cast<double>(args.Delta().Scale);

            if (scale > 0.0 && std::abs(scale - 1.0) > 1.0e-4)
            {
                auto const model = CurrentModel();
                auto const layout = CurrentLayout(model);
                auto const anchor = std::clamp(layout.Scale.TickAt(args.Position().X), 0.0, model.Extent);

                ForgetFit();
                SetView(ZoomAbout(model.View, scale, anchor, model.Limits));
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to zoom the timeline.")
    }

    _Use_decl_annotations_
    void TimelineBar::OnDoubleTapped(input::DoubleTappedRoutedEventArgs const& args)
    {
        try
        {
            auto const hit = HitTest(CurrentLayout(CurrentModel()), args.GetPosition(m_canvas));

            if (hit == Hit::View || hit == Hit::LeftEnd || hit == Hit::RightEnd)
            {
                // The second press began a drag; it ends here, so it can't move the view back.
                if (m_dragging)
                {
                    m_dragging = false;
                    m_pressed = Hit::None;
                    m_frozenScale.reset();
                    m_canvas.ReleasePointerCaptures();
                }

                ToggleFit();
                args.Handled(true);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to zoom the timeline to fit.")
    }

    _Use_decl_annotations_
    void TimelineBar::OnKeyDown(input::KeyRoutedEventArgs const& args)
    {
        try
        {
            auto const key = args.Key();
            auto const control = ControlDown();
            auto const model = CurrentModel();
            auto view = model.View;
            auto handled = true;

            constexpr auto OemPlus = static_cast<VirtualKey>(0xBB);
            constexpr auto OemMinus = static_cast<VirtualKey>(0xBD);

            if (control && (key == VirtualKey::Number0 || key == VirtualKey::NumberPad0))
            {
                if (ShiftDown())
                {
                    if (m_actions.ZoomToSelection) { m_actions.ZoomToSelection(); }
                }
                else if (m_actions.ZoomToFit)
                {
                    m_actions.ZoomToFit();
                }
            }
            else if (control && (key == VirtualKey::Right || key == VirtualKey::Up || key == VirtualKey::Add || key == OemPlus))
            {
                if (m_actions.ZoomStep) { m_actions.ZoomStep(KeyZoom); }
            }
            else if (control && (key == VirtualKey::Left || key == VirtualKey::Down || key == VirtualKey::Subtract || key == OemMinus))
            {
                if (m_actions.ZoomStep) { m_actions.ZoomStep(1.0 / KeyZoom); }
            }
            else if (key == VirtualKey::Left || key == VirtualKey::Right)
            {
                view.Start += (key == VirtualKey::Right ? 0.1 : -0.1) * view.Span;
                SetView(ClampView(view, model.Limits));
            }
            else if (key == VirtualKey::PageUp || key == VirtualKey::PageDown)
            {
                view.Start += (key == VirtualKey::PageDown ? 0.9 : -0.9) * view.Span;
                SetView(ClampView(view, model.Limits));
            }
            else if (key == VirtualKey::Home || key == VirtualKey::End)
            {
                view.Start = key == VirtualKey::Home ? 0.0 : model.Extent;
                SetView(ClampView(view, model.Limits));
            }
            else
            {
                handled = false;
            }

            if (handled)
            {
                ForgetFit();
                args.Handled(true);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to move the timeline from the keyboard.")
    }

    _Use_decl_annotations_
    void TimelineBar::OnRightTapped(input::RightTappedRoutedEventArgs const& args)
    {
        try
        {
            std::weak_ptr<bool> const alive = m_alive;
            controls::MenuFlyout menu{};

            auto const radio = [&](wchar_t const* key, wchar_t const* group, bool checked, std::function<void()> act)
            {
                controls::RadioMenuFlyoutItem item{};
                item.Text(res::GetString(key));
                item.GroupName(group);
                item.IsChecked(checked);
                item.Click([alive, act](auto&&, auto&&)
                {
                    if (alive.lock()) { act(); }
                });
                menu.Items().Append(item);
            };

            radio(L"TimelineBarOverviewItem", L"kind", m_overview, [this]() { if (m_actions.SetOverview) { m_actions.SetOverview(true); } });
            radio(L"TimelineBarPlainItem", L"kind", !m_overview, [this]() { if (m_actions.SetOverview) { m_actions.SetOverview(false); } });
            menu.Items().Append(controls::MenuFlyoutSeparator{});
            radio(L"TimelineBarUnderItem", L"place", !m_atTop, [this]() { if (m_actions.SetAtTop) { m_actions.SetAtTop(false); } });
            radio(L"TimelineBarAboveItem", L"place", m_atTop, [this]() { if (m_actions.SetAtTop) { m_actions.SetAtTop(true); } });
            menu.Items().Append(controls::MenuFlyoutSeparator{});

            auto const command = [&](wchar_t const* key, wchar_t const* accelerator, std::function<void()> act)
            {
                controls::MenuFlyoutItem item{};
                item.Text(res::GetString(key));
                item.KeyboardAcceleratorTextOverride(accelerator);
                item.Click([alive, act](auto&&, auto&&)
                {
                    if (alive.lock()) { act(); }
                });
                menu.Items().Append(item);
            };

            command(L"TimelineZoomFitItem", L"Ctrl+0", [this]() { ForgetFit(); if (m_actions.ZoomToFit) { m_actions.ZoomToFit(); } });
            command(L"TimelineZoomSelectionItem", L"Ctrl+Shift+0", [this]() { ForgetFit(); if (m_actions.ZoomToSelection) { m_actions.ZoomToSelection(); } });

            primitives::FlyoutShowOptions options{};
            options.Position(args.GetPosition(m_canvas));
            menu.ShowAt(m_canvas, options);
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the timeline scroll bar's menu.")
    }
}
