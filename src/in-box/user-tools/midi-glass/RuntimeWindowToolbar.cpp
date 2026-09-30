// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The window a layout runs in: a toolbar window, a see-through one, and one that stays in front.
//
// A toolbar window is the page and nothing else. It has no title bar, no row of buttons and no
// border, so a strip of buttons can sit over another app the way a toolbar does. What the title
// bar would have done lives on a handle at one end: drag it to move the window, click it for the
// menu. Panic is in that menu, and on Ctrl+Shift+P as always.

#include "pch.h"
#include "RuntimeWindow.xaml.h"

#include "StringResources.h"

#include <cmath>
#include <limits>

#include <DispatcherQueue.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <winrt/Windows.UI.Composition.h>

// SetWindowSubclass and DefSubclassProc.
#pragma comment(lib, "comctl32.lib")

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace windowing = ::winrt::Microsoft::UI::Windowing;

        // Room for a fingertip, and for the two marks side by side on a strip a few buttons high.
        constexpr double HandleThickness = 22.0;

        // How far a press on the handle travels before it moves the window instead of opening
        // the menu.
        constexpr double HandleDragThreshold = 4.0;

        // The title bar and the row of buttons above a page, and the narrowest window those
        // buttons still fit in.
        constexpr double StandardChromeHeight = 80.0;
        constexpr double StandardMinimumWidth = 520.0;

        // A window's outer size may be no smaller than this. Windows' own minimum for an
        // ordinary window is about 136 pixels across at 100 %, wider than a slim toolbar.
        constexpr int32_t ToolbarMinimumSide = 16;

        constexpr UINT_PTR ToolbarSubclassId = 1;

        // Windows asks a window how small it may be, and holds a toolbar to its usual minimum
        // unless the window answers for itself.
        LRESULT CALLBACK ToolbarSubclassProcedure(
            _In_ HWND window,
            _In_ UINT message,
            _In_ WPARAM wParam,
            _In_ LPARAM lParam,
            _In_ UINT_PTR subclassId,
            _In_ DWORD_PTR referenceData) noexcept
        {
            UNREFERENCED_PARAMETER(referenceData);

            if (message == WM_GETMINMAXINFO)
            {
                auto const result = ::DefSubclassProc(window, message, wParam, lParam);

                if (auto* const limits = reinterpret_cast<MINMAXINFO*>(lParam))
                {
                    limits->ptMinTrackSize.x = ToolbarMinimumSide;
                    limits->ptMinTrackSize.y = ToolbarMinimumSide;
                }

                return result;
            }

            if (message == WM_NCDESTROY)
            {
                ::RemoveWindowSubclass(window, &ToolbarSubclassProcedure, subclassId);
            }

            return ::DefSubclassProc(window, message, wParam, lParam);
        }

        // What the toolbar menu offers. A toolbar is built at about the size it is used at, so
        // the choices stay near it.
        constexpr int32_t ToolbarSizes[]{ 50, 75, 100, 125, 150, 200 };

        HWND HandleOf(_In_ xaml::Window const& window) noexcept
        {
            HWND handle{ nullptr };

            if (auto const native = window.try_as<::IWindowNative>())
            {
                LOG_IF_FAILED(native->get_WindowHandle(&handle));
            }

            return handle;
        }

        // A window's backdrop has to be a system composition brush, which takes a system
        // compositor, which takes a system dispatcher queue on this thread. One of each serves
        // every window on the thread. Held as raw pointers and never released: a window can
        // still be closing while the process exits, and a C++/WinRT object cannot be made with
        // new.
        winrt::Windows::UI::Composition::CompositionBrush ClearBrush()
        {
            thread_local void* compositorAbi{ nullptr };

            if (compositorAbi == nullptr)
            {
                if (winrt::Windows::System::DispatcherQueue::GetForCurrentThread() == nullptr)
                {
                    DispatcherQueueOptions options{ sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_NONE };
                    ABI::Windows::System::IDispatcherQueueController* controller{ nullptr };

                    winrt::check_hresult(::CreateDispatcherQueueController(options, &controller));
                }

                compositorAbi = winrt::detach_abi(winrt::Windows::UI::Composition::Compositor{});
            }

            winrt::Windows::UI::Composition::Compositor compositor{ nullptr };
            winrt::copy_from_abi(compositor, compositorAbi);

            return compositor.CreateColorBrush(winrt::Windows::UI::Colors::Transparent());
        }
    }

    // ---------------------------------------------------------------- sizes

    bool RuntimeWindow::ToolbarHandleOnLeft() const noexcept
    {
        // Across a wide strip the handle is at its left end; down a tall one, at its top.
        return m_document.PageWidth >= m_document.PageHeight;
    }

    double RuntimeWindow::ToolbarPageScale() const noexcept
    {
        return m_document.Scale == glass::ScaleMode::Custom
            ? std::clamp(m_document.CustomScalePercent / 100.0, 0.1, 4.0)
            : 1.0;
    }

    double RuntimeWindow::WindowScale()
    {
        try
        {
            if (auto const root = RootGrid().XamlRoot())
            {
                if (auto const scale = root.RasterizationScale(); scale > 0.0)
                {
                    return scale;
                }
            }

            // Before the first layout there is no XAML root yet, but there is a window.
            if (auto const handle = HandleOf(*this))
            {
                if (auto const dpi = ::GetDpiForWindow(handle); dpi > 0)
                {
                    return dpi / 96.0;
                }
            }
        }
        catch (...)
        {
        }

        return 1.0;
    }

    winrt::Windows::Graphics::SizeInt32 RuntimeWindow::OpeningClientSize()
    {
        auto const scale = WindowScale();
        auto const pageScale = ToolbarPageScale();
        auto const pageWidth = m_document.PageWidth * pageScale;
        auto const pageHeight = m_document.PageHeight * pageScale;

        if (m_toolbar)
        {
            auto const onLeft = ToolbarHandleOnLeft();

            return
            {
                static_cast<int32_t>(std::lround((pageWidth + (onLeft ? HandleThickness : 0.0)) * scale)),
                static_cast<int32_t>(std::lround((pageHeight + (onLeft ? 0.0 : HandleThickness)) * scale)),
            };
        }

        // Fitting fills whatever window it is given, so it keeps the usual one.
        if (m_document.Scale == glass::ScaleMode::FitToScreen)
        {
            return { 0, 0 };
        }

        // Never bigger than the usual window: a big page scrolls in one, the way it always has.
        auto const width = std::min(
            static_cast<int32_t>(std::lround(std::max(pageWidth + 2.0, StandardMinimumWidth) * scale)),
            DefaultWindowWidth);

        auto const height = std::min(
            static_cast<int32_t>(std::lround((StandardChromeHeight + pageHeight + 2.0) * scale)),
            DefaultWindowHeight);

        if (width >= DefaultWindowWidth && height >= DefaultWindowHeight)
        {
            return { 0, 0 };
        }

        return { width, height };
    }

    bool RuntimeWindow::FitWindowToPage()
    {
        try
        {
            auto const appWindow = AppWindow();
            auto const size = OpeningClientSize();

            if (appWindow == nullptr || size.Width <= 0 || size.Height <= 0)
            {
                return false;
            }

            appWindow.ResizeClient(size);

            // ResizeClient counts a title bar as outside the content. A window whose content
            // runs up under its title bar comes out taller than asked by the title bar's
            // height, so what it came out at is measured and the difference taken back.
            auto const client = appWindow.ClientSize();
            auto const extraWidth = std::max(0, client.Width - size.Width);
            auto const extraHeight = std::max(0, client.Height - size.Height);

            if (extraWidth > 0 || extraHeight > 0)
            {
                auto const outer = appWindow.Size();

                appWindow.Resize({ outer.Width - extraWidth, outer.Height - extraHeight });
            }

            return true;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to fit the window to its page.")

        return false;
    }

    // ---------------------------------------------------------------- style

    void RuntimeWindow::ApplyWindowStyle()
    {
        try
        {
            auto const appWindow = AppWindow();

            if (appWindow == nullptr)
            {
                return;
            }

            auto const toolbar = m_toolbar && !m_fullScreen;
            auto const standard = !m_toolbar && !m_fullScreen;

            auto const shown = [](bool visible)
                {
                    return visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
                };

            AppTitleBar().Visibility(shown(standard));
            ChromeBar().Visibility(shown(standard));
            ChromeFill().Visibility(shown(standard && m_seeThrough));
            ToolbarHandle().Visibility(shown(toolbar));

            if (m_toolbar)
            {
                // The page is the whole window. The shared chrome made the title bar a place to
                // drag the window from, and Windows keeps that place after the title bar is
                // hidden, so without this the top of every button would move the window and a
                // right click there would open the system menu.
                xaml::Window window = *this;

                window.SetTitleBar(nullptr);
                window.ExtendsContentIntoTitleBar(false);

                if (auto const presenter = appWindow.Presenter().try_as<windowing::OverlappedPresenter>())
                {
                    presenter.SetBorderAndTitleBar(false, false);
                    presenter.IsResizable(false);
                    presenter.IsMaximizable(false);
                }

                // Setting it again is harmless: the same procedure and id only update it.
                if (auto const handle = HandleOf(window))
                {
                    ::SetWindowSubclass(handle, &ToolbarSubclassProcedure, ToolbarSubclassId, 0);
                }
            }

            ApplyKeepOnTop(m_keepOnTop);

            auto const onLeft = ToolbarHandleOnLeft();
            auto const automatic = std::numeric_limits<double>::quiet_NaN();

            ToolbarHandle().HorizontalAlignment(onLeft ? xaml::HorizontalAlignment::Left : xaml::HorizontalAlignment::Stretch);
            ToolbarHandle().VerticalAlignment(onLeft ? xaml::VerticalAlignment::Stretch : xaml::VerticalAlignment::Top);
            ToolbarHandle().Width(onLeft ? HandleThickness : automatic);
            ToolbarHandle().Height(onLeft ? automatic : HandleThickness);

            ToolbarHandleIcons().Orientation(onLeft ? controls::Orientation::Vertical : controls::Orientation::Horizontal);

            SurfaceScroll().Margin(!toolbar
                ? xaml::Thickness{ 0, 0, 0, 0 }
                : onLeft
                    ? xaml::Thickness{ HandleThickness, 0, 0, 0 }
                    : xaml::Thickness{ 0, HandleThickness, 0, 0 });

            // The window is cut to the page, so there is nothing to scroll to, and a scroll bar
            // brought in by a pixel of rounding would cover a row of buttons.
            auto const bars = toolbar ? controls::ScrollBarVisibility::Disabled : controls::ScrollBarVisibility::Auto;
            auto const scrolling = toolbar ? controls::ScrollMode::Disabled : controls::ScrollMode::Auto;

            SurfaceScroll().HorizontalScrollBarVisibility(bars);
            SurfaceScroll().VerticalScrollBarVisibility(bars);
            SurfaceScroll().HorizontalScrollMode(scrolling);
            SurfaceScroll().VerticalScrollMode(scrolling);

            if (toolbar)
            {
                FitWindowToPage();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set up the window.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::ApplyKeepOnTop(bool keepOnTop)
    {
        m_keepOnTop = keepOnTop;

        try
        {
            if (auto const appWindow = AppWindow())
            {
                // Full screen is in front already, and has no such setting.
                if (auto const presenter = appWindow.Presenter().try_as<windowing::OverlappedPresenter>())
                {
                    presenter.IsAlwaysOnTop(keepOnTop);
                }
            }

            auto const previous = m_updatingChrome;
            m_updatingChrome = true;

            KeepOnTopToggle().IsChecked(keepOnTop);

            m_updatingChrome = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to keep the window in front.")
    }

    void RuntimeWindow::ApplySeeThrough()
    {
        if (!m_seeThrough)
        {
            return;
        }

        try
        {
            // The app's own material would be drawn behind everything, so it goes first.
            m_chrome.Shutdown();

            // Windows draws a window as one solid block unless it is told the window has parts
            // that can be seen through. Asking for a blur over a region that is not on the
            // window says so without blurring anything.
            if (auto const handle = HandleOf(*this))
            {
                MARGINS const margins{ 0, 0, 0, 0 };
                LOG_IF_FAILED(::DwmExtendFrameIntoClientArea(handle, &margins));

                wil::unique_hrgn region{ ::CreateRectRgn(-2, -2, -1, -1) };

                DWM_BLURBEHIND blur{};
                blur.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
                blur.fEnable = TRUE;
                blur.hRgnBlur = region.get();

                LOG_IF_FAILED(::DwmEnableBlurBehindWindow(handle, &blur));
            }

            xaml::Window window = *this;

            if (auto const target = window.try_as<winrt::Microsoft::UI::Composition::ICompositionSupportsSystemBackdrop>())
            {
                target.SystemBackdrop(ClearBrush());
            }

            WindowFill().Visibility(xaml::Visibility::Collapsed);
            WindowTint().Visibility(xaml::Visibility::Collapsed);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to make the window see-through.")
    }

    void RuntimeWindow::ClearDeckOverlays()
    {
        try
        {
            xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(SurfaceGrain(), nullptr);
            xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(SurfaceDeck(), nullptr);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to clear the deck.")
    }

    // ---------------------------------------------------------------- the handle

    _Use_decl_annotations_
    void RuntimeWindow::OnToolbarHandlePressed(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            auto const point = args.GetCurrentPoint(nullptr);

            // The other button, or a pen's barrel button, is the menu straight away.
            if (point.Properties().IsRightButtonPressed() || point.Properties().IsBarrelButtonPressed())
            {
                args.Handled(true);
                ShowToolbarMenu();
                return;
            }

            if (m_handlePressed || !ToolbarHandle().CapturePointer(args.Pointer()))
            {
                return;
            }

            args.Handled(true);

            m_handlePressed = true;
            m_handleDragging = false;
            m_handlePointerId = args.Pointer().PointerId();
            m_handleStartX = point.Position().X;
            m_handleStartY = point.Position().Y;
            m_handleWindowStart = AppWindow().Position();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to start moving the toolbar.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnToolbarHandleMoved(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_handlePressed || args.Pointer().PointerId() != m_handlePointerId)
        {
            return;
        }

        try
        {
            args.Handled(true);

            auto const point = args.GetCurrentPoint(nullptr);
            auto const scale = WindowScale();

            // Where the pointer is on the screen. The window moves under it, so the pointer's
            // place in the window is added to where the window is now, not where it started.
            auto const window = AppWindow().Position();

            auto const deltaX = (window.X + point.Position().X * scale) - (m_handleWindowStart.X + m_handleStartX * scale);
            auto const deltaY = (window.Y + point.Position().Y * scale) - (m_handleWindowStart.Y + m_handleStartY * scale);

            if (!m_handleDragging && std::hypot(deltaX, deltaY) < HandleDragThreshold * scale)
            {
                return;
            }

            m_handleDragging = true;

            AppWindow().Move({
                m_handleWindowStart.X + static_cast<int32_t>(std::lround(deltaX)),
                m_handleWindowStart.Y + static_cast<int32_t>(std::lround(deltaY)) });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to move the toolbar.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnToolbarHandleReleased(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_handlePressed || args.Pointer().PointerId() != m_handlePointerId)
        {
            return;
        }

        args.Handled(true);

        auto const moved = m_handleDragging;

        EndToolbarHandleGesture();

        try
        {
            ToolbarHandle().ReleasePointerCapture(args.Pointer());
        }
        catch (...)
        {
        }

        // A press that went nowhere is a click, and a click on the handle is the menu.
        if (!moved)
        {
            ShowToolbarMenu();
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnToolbarHandleCaptureLost(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        EndToolbarHandleGesture();
    }

    void RuntimeWindow::EndToolbarHandleGesture()
    {
        m_handlePressed = false;
        m_handleDragging = false;
        m_handlePointerId = 0;
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnToolbarHandleKeyDown(
        foundation::IInspectable const& sender,
        xaml::Input::KeyRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        auto const key = args.Key();

        if (key == winrt::Windows::System::VirtualKey::Enter ||
            key == winrt::Windows::System::VirtualKey::Space ||
            key == winrt::Windows::System::VirtualKey::Application)
        {
            args.Handled(true);
            ShowToolbarMenu();
        }
    }

    void RuntimeWindow::ShowToolbarMenu()
    {
        try
        {
            controls::Primitives::FlyoutShowOptions options{};

            // Beside the strip rather than over its buttons: below a strip across the screen,
            // and to the right of one down it.
            options.Placement(ToolbarHandleOnLeft()
                ? controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft
                : controls::Primitives::FlyoutPlacementMode::RightEdgeAlignedTop);

            ToolbarFlyout().ShowAt(ToolbarHandle(), options);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the toolbar menu.")
    }

    // ---------------------------------------------------------------- the menu

    _Use_decl_annotations_
    void RuntimeWindow::OnToolbarFlyoutOpening(
        foundation::IInspectable const& sender,
        foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            FillPagesMenu(ToolbarPagesItem());
            FillSizeMenu();

            ToolbarKeepOnTopItem().IsChecked(m_keepOnTop);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to fill the toolbar menu.")
    }

    void RuntimeWindow::FillSizeMenu()
    {
        auto const items = ToolbarSizeItem().Items();
        items.Clear();

        auto const current = static_cast<int32_t>(std::lround(ToolbarPageScale() * 100.0));

        for (auto const percent : ToolbarSizes)
        {
            controls::RadioMenuFlyoutItem item{};

            item.GroupName(L"ToolbarSize");
            item.Text(resources::FormatString(L"CanvasScaleFormat", std::to_wstring(percent)));
            item.IsChecked(percent == current);

            auto weak = get_weak();

            item.Click([weak, percent](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->SetToolbarSize(percent);
                    }
                });

            items.Append(item);
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::SetToolbarSize(int32_t percent)
    {
        try
        {
            m_document.Scale = percent == 100 ? glass::ScaleMode::ActualSize : glass::ScaleMode::Custom;

            if (percent != 100)
            {
                m_document.CustomScalePercent = percent;
            }

            // The size box is hidden in a toolbar, but it is what full screen goes back to.
            auto const previous = m_updatingChrome;
            m_updatingChrome = true;

            ScaleSelector().SelectedIndex(static_cast<int32_t>(m_document.Scale));

            m_updatingChrome = previous;

            if (m_toolbar && !m_fullScreen)
            {
                FitWindowToPage();
            }

            ApplyScale();
            RememberScale();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to resize the toolbar.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnToolbarKeepOnTopClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (auto const item = sender.try_as<controls::ToggleMenuFlyoutItem>())
        {
            ApplyKeepOnTop(item.IsChecked());
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnToolbarCloseClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            Close();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to close the toolbar.")
    }
}
