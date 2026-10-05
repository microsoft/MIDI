// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The panels beside the canvas: dragging the bars between them, sizing the palette's tiles to
// the palette, and sizing the step dialog to the window.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogSizing.h"

namespace patchbay = ::midipatchbay;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // Narrow enough to be worth dragging to, wide enough to still be a panel.
        constexpr double MinimumPaletteWidth = 200.0;
        constexpr double MaximumPaletteWidth = 440.0;
        constexpr double MinimumInspectorWidth = 240.0;
        constexpr double MaximumInspectorWidth = 600.0;

        // Whatever the panels are dragged to, the canvas keeps this much.
        constexpr double MinimumCanvasWidth = 240.0;

        // The canvas row's margins and the two bars, which neither panel can take.
        constexpr double FixedRowWidth = 32.0 + 2 * 12.0;

        // A step dialog wider than this only makes its lines longer, not fewer.
        constexpr double MaximumBlockDialogWidth = 1100.0;

        std::wstring TagOf(_In_ foundation::IInspectable const& sender)
        {
            auto const element = sender.try_as<xaml::FrameworkElement>();

            return element == nullptr
                ? std::wstring{}
                : std::wstring{ winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"") };
        }

        // The accent line sits on the plain one and fades in, so both keep following the theme.
        void HighlightSplitter(_In_ foundation::IInspectable const& sender, _In_ bool on)
        {
            auto const border = sender.try_as<controls::Border>();
            auto const lines = border == nullptr ? nullptr : border.Child().try_as<controls::Grid>();

            if (lines != nullptr && lines.Children().Size() > 1)
            {
                lines.Children().GetAt(1).Opacity(on ? 1.0 : 0.0);
            }
        }
    }

    void MainWindow::InitializeSplitters() noexcept
    {
        try
        {
            // ProtectedCursor is not on the projected type, but the interface carrying it is.
            for (auto const& splitter : { PaletteSplitter(), InspectorSplitter() })
            {
                if (auto const element = splitter.try_as<xaml::IUIElementProtected>())
                {
                    element.ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(
                        winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeWestEast));
                }
            }

            auto const& settings = patchbay::AppSettings::Current();

            if (auto const width = settings.EditorPaletteWidth(); width > 0)
            {
                PalettePanel().Width(std::clamp(static_cast<double>(width), MinimumPaletteWidth, MaximumPaletteWidth));
            }

            if (auto const width = settings.EditorInspectorWidth(); width > 0)
            {
                InspectorPanel().Width(std::clamp(static_cast<double>(width), MinimumInspectorWidth, MaximumInspectorWidth));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up the resize bars.")
    }

    _Use_decl_annotations_
    void MainWindow::SetInspectorVisible(bool visible) noexcept
    {
        try
        {
            auto const visibility = visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;

            InspectorPanel().Visibility(visibility);
            InspectorSplitter().Visibility(visibility);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show or hide the details panel.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSplitterPressed(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const element = sender.try_as<xaml::UIElement>();

            if (element == nullptr)
            {
                return;
            }

            m_splitterTag = TagOf(sender);

            // Against the window, not the bar: the bar moves under the pointer as it is dragged.
            auto const point = args.GetCurrentPoint(RootGrid());

            m_splitterStartX = point.Position().X;
            m_splitterStartWidth = m_splitterTag == L"palette"
                ? PalettePanel().ActualWidth()
                : InspectorPanel().ActualWidth();

            m_splitterPointerId = point.PointerId();
            m_draggingSplitter = element.CapturePointer(args.Pointer());

            args.Handled(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start resizing a panel.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSplitterMoved(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
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

            auto const delta = static_cast<double>(point.Position().X) - m_splitterStartX;
            auto const palette = m_splitterTag == L"palette";

            // Never so wide that the canvas is squeezed out.
            auto const other = palette
                ? (InspectorPanel().Visibility() == xaml::Visibility::Visible ? InspectorPanel().ActualWidth() : 0.0)
                : PalettePanel().ActualWidth();

            auto const room = ContentRoot().ActualWidth() - FixedRowWidth - other - MinimumCanvasWidth;

            if (palette)
            {
                auto const maximum = (std::max)(MinimumPaletteWidth, (std::min)(MaximumPaletteWidth, room));

                PalettePanel().Width(std::clamp(m_splitterStartWidth + delta, MinimumPaletteWidth, maximum));
            }
            else
            {
                // The details panel is to the right of its bar, so it grows as the pointer moves left.
                auto const maximum = (std::max)(MinimumInspectorWidth, (std::min)(MaximumInspectorWidth, room));

                InspectorPanel().Width(std::clamp(m_splitterStartWidth - delta, MinimumInspectorWidth, maximum));
            }

            args.Handled(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to resize a panel.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSplitterReleased(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
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

            HighlightSplitter(sender, false);

            // Every editor opens with the widths last dragged to, the same as its place on screen.
            patchbay::AppSettings::Current().EditorPaneWidths(
                static_cast<int32_t>(std::lround(PalettePanel().Width())),
                static_cast<int32_t>(std::lround(InspectorPanel().Width())));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish resizing a panel.")
    }

    // A bar that looks exactly like the line beside it gives nobody a reason to try dragging it,
    // so it takes the resize cursor and lights up under the pointer.
    _Use_decl_annotations_
    void MainWindow::OnSplitterEntered(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            HighlightSplitter(sender, true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to highlight a resize bar.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSplitterExited(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (!m_draggingSplitter)
            {
                HighlightSplitter(sender, false);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to clear a resize bar highlight.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPaletteContentSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        SizePaletteTiles();
    }

    void MainWindow::SizePaletteTiles() noexcept
    {
        try
        {
            auto const content = PaletteContent();
            auto const padding = content.Padding();
            auto const width = content.ActualWidth() - padding.Left - padding.Right;

            if (width <= 0)
            {
                return;
            }

            // Each slot holds a tile and its right margin, and the panel never lets the last
            // margin hang past the edge, so three whole slots fit inside the grid. Square, so a
            // row of tiles lines up however long their names are.
            for (auto const& grid : m_paletteGrids)
            {
                auto const room = grid.ActualWidth() > 0 ? grid.ActualWidth() : width;
                auto const slot = std::floor(room / 3.0);

                if (auto const panel = grid.ItemsPanelRoot().try_as<controls::ItemsWrapGrid>())
                {
                    panel.ItemWidth(slot);
                    panel.ItemHeight(slot);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to size the palette tiles.")
    }

    void MainWindow::FitBlockDialogToWindow() noexcept
    {
        try
        {
            auto const space = midiapp::DialogContentSpace(Content().XamlRoot());

            BlockDialogBody().Width((std::min)(static_cast<double>(space.Width), MaximumBlockDialogWidth));
            BlockDialogBody().MaxHeight(space.Height);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to size the step dialog.")
    }
}
