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

namespace winrt::midiglass::implementation
{
    namespace
    {
        // Narrow enough to be worth dragging to, wide enough to still be a pane.
        constexpr double MinimumPaneWidth = 150.0;
        constexpr double MaximumPaneWidth = 520.0;

        constexpr double MinimumMonitorHeight = 28.0;
        constexpr double MaximumMonitorHeight = 420.0;

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
