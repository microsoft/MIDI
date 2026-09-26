// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The panes: dragging the dividers between them, and folding the left one away.
//
// WinUI has no splitter, so a divider here is a four pixel Border with pointer handlers. That is
// the whole of it: capture the pointer, add the delta to the column or row it governs, and clamp.
// The instinct to grab a divider and give the monitor more room is the right instinct, and a
// divider that does not move looks broken.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "AppSettings.h"

namespace winrt::midiglass::implementation
{
    namespace
    {
        // Narrow enough to be worth dragging to, wide enough to still be a pane.
        constexpr double MinimumPaneWidth = 150.0;
        constexpr double MaximumPaneWidth = 520.0;

        constexpr double MinimumMonitorHeight = 28.0;
        constexpr double MaximumMonitorHeight = 420.0;

        // The same bounds the canvas enforces, so a remembered zoom cannot come back as one the
        // zoom buttons would refuse.
        constexpr double MinimumRememberedScale = 0.1;
        constexpr double MaximumRememberedScale = 2.0;

        // What the designer opens at the very first time, before there is anything to remember.
        constexpr int32_t DefaultEditorWidth = 1500;
        constexpr int32_t DefaultEditorHeight = 950;

        // The width the left pane folds down to: one column of icon buttons.
        constexpr double CollapsedRailWidth = 40.0;

        std::wstring TagOf(_In_ foundation::IInspectable const& sender)
        {
            auto const element = sender.try_as<xaml::FrameworkElement>();

            if (element == nullptr)
            {
                return {};
            }

            return std::wstring{ winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"") };
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnSplitterPressed(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const element = sender.try_as<xaml::UIElement>();

            if (element == nullptr)
            {
                return;
            }

            m_splitterTag = TagOf(sender);

            // Against the window, not against the divider: the divider moves under the pointer
            // as it is dragged, so measuring against it would fight itself.
            auto const point = args.GetCurrentPoint(RootGrid());

            m_splitterStartX = point.Position().X;
            m_splitterStartY = point.Position().Y;

            m_splitterStartWidth = m_splitterTag == L"left"
                ? LeftColumn().ActualWidth()
                : InspectorColumn().ActualWidth();

            m_splitterStartHeight = MonitorRow().ActualHeight();

            m_splitterPointerId = point.PointerId();
            m_draggingSplitter = element.CapturePointer(args.Pointer());
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to start a pane drag.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnSplitterMoved(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_draggingSplitter)
        {
            return;
        }

        try
        {
            auto const point = args.GetCurrentPoint(RootGrid());

            if (point.PointerId() != m_splitterPointerId)
            {
                return;
            }

            if (m_splitterTag == L"left")
            {
                auto const width = std::clamp(
                    m_splitterStartWidth + (point.Position().X - m_splitterStartX),
                    MinimumPaneWidth,
                    MaximumPaneWidth);

                LeftColumn().Width(xaml::GridLengthHelper::FromPixels(width));
            }
            else if (m_splitterTag == L"inspector")
            {
                // The inspector is to the RIGHT of its divider, so it grows as the pointer
                // moves left.
                auto const width = std::clamp(
                    m_splitterStartWidth - (point.Position().X - m_splitterStartX),
                    MinimumPaneWidth,
                    MaximumPaneWidth);

                InspectorColumn().Width(xaml::GridLengthHelper::FromPixels(width));
            }
            else if (m_splitterTag == L"monitor")
            {
                auto const height = std::clamp(
                    m_splitterStartHeight - (point.Position().Y - m_splitterStartY),
                    MinimumMonitorHeight,
                    MaximumMonitorHeight);

                m_monitorHeight = height;

                MonitorRow().Height(xaml::GridLengthHelper::FromPixels(height));
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to drag a pane divider.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnSplitterReleased(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (auto const element = sender.try_as<xaml::UIElement>())
            {
                element.ReleasePointerCapture(args.Pointer());
            }

            if (!m_draggingSplitter)
            {
                return;
            }

            m_draggingSplitter = false;

            // The canvas fits what it is given, so it has to be told the room changed.
            ApplyCanvasScale();
            RebuildGrid();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to finish a pane drag.")
    }

    // A divider that looks exactly like the line beside it gives nobody a reason to try dragging
    // it, so it takes the system resize cursor and its hairline brightens under the pointer.
    //
    // ProtectedCursor is protected on UIElement and so is not on the projected type, but the
    // interface that carries it is projected and the object implements it, so it can be reached
    // by QueryInterface without subclassing every divider.
    _Use_decl_annotations_
    void EditorWindow::InitializeSplitters()
    {
        try
        {
            auto const apply = [](xaml::UIElement const& element, bool horizontal)
                {
                    if (auto const protectedElement = element.try_as<xaml::IUIElementProtected>())
                    {
                        protectedElement.ProtectedCursor(
                            winrt::Microsoft::UI::Input::InputSystemCursor::Create(
                                horizontal
                                    ? winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeWestEast
                                    : winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeNorthSouth));
                    }
                };

            apply(LeftSplitter(), true);
            apply(InspectorSplitter(), true);
            apply(MonitorSplitter(), false);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to give the dividers a resize cursor.")
    }

    // ---------------------------------------------------------------- remembering the place

    _Use_decl_annotations_
    void EditorWindow::RestoreWindowPlacement(int32_t cascadeOffset)
    {
        try
        {
            auto saved = ::midiglass::AppSettings::Current().EditorPlacement();

            // A cascade is only worth anything on a window that is not maximized, and only when
            // there is a saved position to move away from.
            if (saved.Valid && cascadeOffset > 0 && !saved.Maximized)
            {
                saved.X += cascadeOffset;
                saved.Y += cascadeOffset;
            }

            midiapp::WindowChrome::RestorePlacement(
                *this, saved, DefaultEditorWidth, DefaultEditorHeight);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to restore the editor window placement.")
    }

    // The dividers and the zoom are part of where somebody left off, the same as the window
    // position. Restoring one without the other still means rearranging before starting.
    void EditorWindow::RestoreEditorPanes()
    {
        try
        {
            auto const& settings = ::midiglass::AppSettings::Current();

            if (auto const width = settings.EditorLeftPaneWidth(); width > 0)
            {
                LeftColumn().Width(xaml::GridLengthHelper::FromPixels(
                    std::clamp(static_cast<double>(width), MinimumPaneWidth, MaximumPaneWidth)));
            }

            if (auto const width = settings.EditorInspectorWidth(); width > 0)
            {
                InspectorColumn().Width(xaml::GridLengthHelper::FromPixels(
                    std::clamp(static_cast<double>(width), MinimumPaneWidth, MaximumPaneWidth)));
            }

            if (auto const height = settings.EditorMonitorHeight(); height > 0)
            {
                m_monitorHeight = std::clamp(
                    static_cast<double>(height), MinimumMonitorHeight, MaximumMonitorHeight);

                MonitorRow().Height(xaml::GridLengthHelper::FromPixels(m_monitorHeight));
            }

            // Zero means Fit, which is what a layout opens at unless somebody chose otherwise.
            if (auto const zoom = settings.EditorZoomPercent(); zoom > 0)
            {
                m_zoomIsFit = false;
                m_canvasScale = std::clamp(zoom / 100.0, MinimumRememberedScale, MaximumRememberedScale);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to restore the editor panes.")
    }

    void EditorWindow::SaveEditorPlacement()
    {
        try
        {
            auto& settings = ::midiglass::AppSettings::Current();

            auto const placement = midiapp::WindowChrome::CapturePlacement(*this);

            if (placement.Valid)
            {
                settings.EditorPlacement(placement);
            }

            // The left pane may be folded away right now, so the width it folds back to is what
            // gets kept, not the rail.
            auto const folded = LeftRail() != nullptr &&
                LeftRail().Visibility() == xaml::Visibility::Visible;

            auto const leftWidth = folded ? m_leftPaneWidth : LeftColumn().ActualWidth();

            settings.EditorPaneSizes(
                static_cast<int32_t>(std::lround(leftWidth)),
                static_cast<int32_t>(std::lround(InspectorColumn().ActualWidth())),
                static_cast<int32_t>(std::lround(m_monitorHeight)),
                m_zoomIsFit ? 0 : static_cast<int32_t>(std::lround(m_canvasScale * 100.0)));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to remember where the editor was.")
    }

    namespace
    {
        void PaintSplitterLine(_In_ foundation::IInspectable const& sender, _In_ wchar_t const* key)
        {
            auto const border = sender.try_as<controls::Border>();

            if (border == nullptr)
            {
                return;
            }

            if (auto const line = border.Child().try_as<::winrt::Microsoft::UI::Xaml::Shapes::Rectangle>())
            {
                line.Fill(xaml::Application::Current().Resources()
                    .Lookup(box_value(key)).as<media::Brush>());
            }
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnSplitterEntered(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            PaintSplitterLine(sender, L"AccentFillColorDefaultBrush");
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to highlight a divider.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnSplitterExited(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (m_draggingSplitter)
            {
                return;
            }

            PaintSplitterLine(sender, L"DividerStrokeColorDefaultBrush");
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to clear a divider highlight.")
    }

    // ---------------------------------------------------------------- folding the left pane

    _Use_decl_annotations_
    void EditorWindow::OnCollapseLeftPaneClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            m_leftPaneWidth = LeftColumn().ActualWidth();

            LeftPaneBody().Visibility(xaml::Visibility::Collapsed);
            LeftRail().Visibility(xaml::Visibility::Visible);
            LeftSplitter().Visibility(xaml::Visibility::Collapsed);

            LeftColumn().Width(xaml::GridLengthHelper::FromPixels(CollapsedRailWidth));

            ApplyCanvasScale();
            RebuildGrid();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to fold the left pane away.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnExpandLeftPaneClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            LeftRail().Visibility(xaml::Visibility::Collapsed);
            LeftPaneBody().Visibility(xaml::Visibility::Visible);
            LeftSplitter().Visibility(xaml::Visibility::Visible);

            LeftColumn().Width(xaml::GridLengthHelper::FromPixels(
                m_leftPaneWidth > CollapsedRailWidth ? m_leftPaneWidth : 190.0));

            // Opening on the tab that was asked for, so getting back to the outline is one click
            // rather than one click and then a hunt.
            auto const tag = TagOf(sender);

            SelectLeftTab(tag == L"1" ? 1 : 0);

            ApplyCanvasScale();
            RebuildGrid();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the left pane.")
    }
}
