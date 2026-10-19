// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The timeline's scroll bar (design section 31), drawn with Win2D: an overview of the whole
// sequence, a line for each track where its clips are and the sections by name, or a plain bar.
// Either way, drag the view to scroll, drag an end to zoom, drag the view up or down to zoom,
// Ctrl + wheel or a pinch zooms about the pointer, and double-click the view to fit and back. A
// switch at its left end changes the kind, and keys at its right end zoom. A theme's parts
// (timeline.bar.*) draw it; without them it's drawn from the palette. It knows nothing about the
// sequence: the window gives it a model to draw, and the views it asks for.

#include "ThemePalette.h"
#include "ThemePainter.h"
#include "TimelineView.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace midisequencer
{
    struct TimelineBarLine
    {
        Color Color{};
        bool Muted{ false };
        std::vector<std::pair<int64_t, int64_t>> Spans{};
    };

    struct TimelineBarSection
    {
        int64_t Tick{ 0 };
        std::wstring Name{};
        std::optional<Color> Color{};
    };

    // What the bar draws, asked for each time it's drawn.
    struct TimelineBarModel
    {
        TimelineSpan View{};
        TimelineLimits Limits{};
        double Extent{ 1 };
        int64_t TicksPerBar{ 3840 };

        // Where the timeline's column is in the bar's row, and how wide the track list's is.
        float LaneLeft{ 0 };
        float LaneWidth{ 1 };
        float HeaderWidth{ 0 };

        int64_t Playhead{ -1 };
        int64_t LoopStart{ 0 };
        int64_t LoopEnd{ 0 };
        bool LoopOn{ false };

        std::vector<TimelineBarLine> Lines{};
        std::vector<TimelineBarSection> Sections{};

        // The selected clip and its linked copies; the selected one first.
        std::vector<std::pair<int64_t, int64_t>> Linked{};
    };

    struct TimelineBarActions
    {
        std::function<TimelineBarModel()> Model{};
        std::function<void(TimelineSpan const&)> SetView{};

        // The keys and the key commands: about the playhead when it's in view, else the middle.
        std::function<void(double factor)> ZoomStep{};
        std::function<void()> ZoomToFit{};
        std::function<void()> ZoomToSelection{};

        std::function<void(bool)> SetOverview{};
        std::function<void(bool)> SetAtTop{};
    };

    class TimelineBar
    {
        using PaintRect = themeengine::PaintRect;

    public:
        TimelineBar() = default;
        ~TimelineBar();
        TimelineBar(TimelineBar const&) = delete;
        TimelineBar& operator=(TimelineBar const&) = delete;

        // The element to put in the window's grid.
        winrt::Microsoft::UI::Xaml::Controls::Grid Create(_In_ TimelineBarActions actions);

        winrt::Microsoft::UI::Xaml::Controls::Grid Host() const noexcept { return m_host; }

        // The colors and the theme's parts, from the window's palette, which outlives the bar.
        void Palette(_In_ midisequencer::Palette const* palette) noexcept;

        void Overview(_In_ bool value) noexcept;
        bool Overview() const noexcept { return m_overview; }
        void AtTop(_In_ bool value) noexcept;
        bool AtTop() const noexcept { return m_atTop; }

        // The row's height for the kind of bar and the theme.
        double RowHeight() const noexcept;

        void Invalidate() noexcept;

        // Drawn again only when the playhead moves a pixel along the bar.
        void PlayheadMoved(_In_ int64_t tick) noexcept;

        // Zoom to fit, or back to what was in view before, as double-clicking the view does.
        void ToggleFit() noexcept;

        // The view moved some other way, so double-clicking fits again rather than going back.
        void ForgetFit() noexcept { m_beforeFit.reset(); }

    private:
        enum class Hit : uint8_t
        {
            None,
            Plain,
            Overview,
            ZoomOut,
            ZoomIn,
            ZoomFit,
            Track,
            View,
            LeftEnd,
            RightEnd,
        };

        struct Layout
        {
            PaintRect Toggle{};
            PaintRect Segments[2]{};
            PaintRect Keys[3]{};
            PaintRect Separator{};
            PaintRect Track{};
            TimelineBarScale Scale{};
            float ViewLeft{ 0 };
            float ViewRight{ 0 };
        };

        Layout Arrange(_In_ TimelineBarModel const& model, _In_ float width, _In_ float height) const;
        Hit HitTest(_In_ Layout const& layout, _In_ winrt::Windows::Foundation::Point const& point) const noexcept;
        float Metric(_In_ std::wstring_view name, _In_ float fallback) const;
        std::shared_ptr<themeengine::ResolvedTheme const> Theme() const noexcept;

        void Draw(
            _In_ winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ float width,
            _In_ float height);

        void DrawToggle(
            _In_ winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ Layout const& layout);

        void DrawKeys(
            _In_ winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ Layout const& layout);

        void DrawOverview(
            _In_ winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ Layout const& layout,
            _In_ TimelineBarModel const& model);

        void DrawPlain(
            _In_ winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ Layout const& layout,
            _In_ TimelineBarModel const& model);

        // A theme's part painted in the rectangle, if the theme has it.
        bool PaintPart(
            _In_ winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            _In_ winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            _In_ std::wstring_view part,
            _In_ std::vector<std::wstring_view> const& states,
            _In_ PaintRect const& rect);

        void OnPressed(_In_ winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnMoved(_In_ winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnReleased(_In_ winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args, _In_ bool lost);
        void OnWheel(_In_ winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnKeyDown(_In_ winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void OnDoubleTapped(_In_ winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& args);
        void OnRightTapped(_In_ winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);
        void OnPinch(_In_ winrt::Microsoft::UI::Xaml::Input::ManipulationDeltaRoutedEventArgs const& args);

        void SetHover(_In_ Hit hit);
        void SetView(_In_ TimelineSpan const& view);
        TimelineBarModel CurrentModel() const;
        Layout CurrentLayout(_In_ TimelineBarModel const& model) const;
        void UpdateName(_In_ TimelineBarModel const& model);

        TimelineBarActions m_actions{};
        midisequencer::Palette const* m_palette{ nullptr };

        winrt::Microsoft::UI::Xaml::Controls::Grid m_host{ nullptr };
        winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl m_canvas{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ToolTip m_tip{ nullptr };
        themeengine::ThemePainter m_painter{};

        winrt::Microsoft::Graphics::Canvas::Text::CanvasTextFormat m_sectionFormat{ nullptr };
        winrt::Microsoft::Graphics::Canvas::Text::CanvasTextFormat m_glyphFormat{ nullptr };
        std::wstring m_sectionFont{};

        bool m_overview{ true };
        bool m_atTop{ false };

        Hit m_hover{ Hit::None };
        Hit m_pressed{ Hit::None };
        bool m_dragging{ false };
        uint32_t m_pointerId{ 0 };
        winrt::Windows::Foundation::Point m_pressPoint{};
        TimelineSpan m_pressView{};
        TimelineLimits m_pressLimits{};
        double m_pressOffset{ 0 };

        // While dragging, the bar keeps the scale it had when the drag began, so what's under the
        // pointer stays under it as the view grows past the sequence.
        std::optional<TimelineBarScale> m_frozenScale{};

        std::optional<TimelineSpan> m_beforeFit{};

        // The handlers check this before they touch the bar, which can go before its elements do.
        std::shared_ptr<bool> m_alive{ std::make_shared<bool>(true) };
        float m_playheadX{ -1 };
        std::wstring m_name{};
    };
}
