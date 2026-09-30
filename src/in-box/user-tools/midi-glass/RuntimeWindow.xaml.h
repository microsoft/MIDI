// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "RuntimeWindow.g.h"

#include "WindowChrome.h"
#include "LayoutModel.h"
#include "ThemeModel.h"
#include "LivePlayer.h"
#include "SurfaceRenderer.h"
#include "DeckBrush.h"
#include "InputRouter.h"

namespace winrt::midiglass::implementation
{
    // One running layout. Several of these live in the one process, which is what lets two
    // layouts share a connection to the same instrument and what makes Panic mean "everything
    // this app is driving".
    struct RuntimeWindow : RuntimeWindowT<RuntimeWindow>
    {
        RuntimeWindow() = default;

        // Not projected. Called before Activate, so the window is sized and placed, and the
        // layout is read, before its first paint.
        bool LoadLayout(_In_ std::wstring const& filePath);

        std::wstring const& LayoutFilePath() const noexcept { return m_filePath; }

        void OnRootLoaded(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnPageSelectionChanged(
            foundation::IInspectable const& sender,
            controls::SelectionChangedEventArgs const& args);

        void OnScaleSelectionChanged(
            foundation::IInspectable const& sender,
            controls::SelectionChangedEventArgs const& args);

        void OnKeepOnTopChecked(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnKeepOnTopUnchecked(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnFullScreenClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnPanicClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        // ---- full screen (RuntimeWindowFullScreen.cpp) ----

        void OnCornerButtonPointerEntered(
            foundation::IInspectable const& sender,
            xaml::Input::PointerRoutedEventArgs const& args);
        void OnCornerButtonPointerExited(
            foundation::IInspectable const& sender,
            xaml::Input::PointerRoutedEventArgs const& args);
        void OnCornerFlyoutOpening(
            foundation::IInspectable const& sender,
            foundation::IInspectable const& args);
        void OnCornerMenuClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnFullScreenAccelerator(
            xaml::Input::KeyboardAccelerator const& sender,
            xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        void OnEscapeAccelerator(
            xaml::Input::KeyboardAccelerator const& sender,
            xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        void OnPanicAccelerator(
            xaml::Input::KeyboardAccelerator const& sender,
            xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        void OnSurfaceSizeChanged(
            foundation::IInspectable const& sender,
            xaml::SizeChangedEventArgs const& args);

        // ---- the toolbar window (RuntimeWindowToolbar.cpp) ----

        void OnToolbarHandlePressed(
            foundation::IInspectable const& sender,
            xaml::Input::PointerRoutedEventArgs const& args);
        void OnToolbarHandleMoved(
            foundation::IInspectable const& sender,
            xaml::Input::PointerRoutedEventArgs const& args);
        void OnToolbarHandleReleased(
            foundation::IInspectable const& sender,
            xaml::Input::PointerRoutedEventArgs const& args);
        void OnToolbarHandleCaptureLost(
            foundation::IInspectable const& sender,
            xaml::Input::PointerRoutedEventArgs const& args);
        void OnToolbarHandleKeyDown(
            foundation::IInspectable const& sender,
            xaml::Input::KeyRoutedEventArgs const& args);
        void OnToolbarFlyoutOpening(
            foundation::IInspectable const& sender,
            foundation::IInspectable const& args);
        void OnToolbarKeepOnTopClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);
        void OnToolbarCloseClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

    private:
        // The usual window, in physical pixels, for a page that does not fit a smaller one.
        static constexpr int32_t DefaultWindowWidth = 1320;
        static constexpr int32_t DefaultWindowHeight = 900;

        void OnWindowClosed(
            foundation::IInspectable const& sender,
            xaml::WindowEventArgs const& args);

        // ---- surface ----

        void BuildPage(_In_ size_t pageIndex);

        // Changes page and keeps the page selector in step. What a page tab and a sequence step
        // both go through, so there is one path and one place a page change can go wrong.
        void ShowPage(_In_ size_t pageIndex);

        void ApplyScale();
        void UpdateDeckBrush();
        void SetFullScreen(_In_ bool fullScreen);
        void Panic();

        // The one button and its flyout, and the note that says how to get out again.
        void ShowFullScreenChrome();
        void ApplyCornerButtonPlacement();
        void ShowEscapeToast();

        // One entry per page, for the corner menu and the toolbar menu alike.
        void FillPagesMenu(_In_ controls::MenuFlyoutSubItem const& pages);

        // ---- the window itself (RuntimeWindowToolbar.cpp) ----

        // The size a layout opens at: the page and its handle for a toolbar, and a window that
        // fits a small page rather than the usual big one otherwise.
        winrt::Windows::Graphics::SizeInt32 OpeningClientSize();

        // The title bar and buttons, or the toolbar's handle, and whether it stays in front.
        // Run again after full screen, because leaving it brings back an ordinary window.
        void ApplyWindowStyle();
        void ApplyKeepOnTop(_In_ bool keepOnTop);

        // Nothing drawn behind the controls: no deck, and no window.
        void ApplySeeThrough();
        void ClearDeckOverlays();

        // Sizes the window to OpeningClientSize. False when the page wants the usual window.
        bool FitWindowToPage();

        // The toolbar menu's sizes. The window is the page, so a size is the window's size.
        void FillSizeMenu();
        void SetToolbarSize(_In_ int32_t percent);

        // The last scale used is kept in the layout, so it opens that way next time.
        void RememberScale();

        bool ToolbarHandleOnLeft() const noexcept;
        double ToolbarPageScale() const noexcept;
        double WindowScale();
        void ShowToolbarMenu();
        void EndToolbarHandleGesture();

        // A performer's screen must not blank mid set.
        void HoldDisplayAwake(_In_ bool hold);

        // ---- devices and sending ----

        void StartDevices();
        void UpdateDeviceStatus();
        void MarkUnreachableControls(_In_ std::vector<glass::ResolvedDevice> const& devices);

        void OnControlValueChanged(_In_ size_t itemIndex, _In_ double value, _In_ bool isFinal);
        void OnControlValueYChanged(_In_ size_t itemIndex, _In_ double value, _In_ bool isFinal);
        void OnControlSetDirectly(_In_ size_t itemIndex, _In_ double value);
        void OnControlSwitched(_In_ size_t itemIndex, _In_ bool isOn, _In_ double velocity);
        void OnControlTouched(_In_ size_t itemIndex, _In_ bool isTouched);
        void OnControlKeyChanged(
            _In_ size_t itemIndex,
            _In_ int32_t key,
            _In_ double velocity,
            _In_ bool isDown);
        void OnControlPadTouched(_In_ size_t itemIndex, _In_ glass::PadTouch const& touch);

        void OnFeedbackMoved(_In_ uint32_t controlIndex, _In_ double value);
        void OnActivitySeen(
            _In_ uint32_t controlIndex,
            _In_ glass::LivePlayer::ListenerState state);
        void OnBeatMoved(
            _In_ uint32_t controlIndex,
            _In_ int32_t beatInBar,
            _In_ double phase,
            _In_ bool running);

        midiapp::WindowChrome m_chrome{};

        std::wstring m_filePath{};
        glass::LayoutDocument m_document{};
        glass::Theme m_theme{};

        size_t m_pageIndex{ 0 };

        glass::SurfaceRenderer m_renderer{};
        glass::InputRouter m_input{};

        // The device table, the connections and the binding engine. Shared with the editor's
        // Try mode, which drives one of these too.
        std::shared_ptr<glass::LivePlayer> m_player{};

        std::wstring m_ownerId{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_updatingChrome{ false };
        bool m_fullScreen{ false };

        // The corner button settles back to a quarter opacity once it has been seen, unless the
        // pointer is on it.
        xaml::DispatcherTimer m_cornerFadeTimer{ nullptr };
        xaml::DispatcherTimer m_toastTimer{ nullptr };
        bool m_cornerHovered{ false };

        winrt::Windows::System::Display::DisplayRequest m_displayRequest{ nullptr };
        bool m_displayHeld{ false };

        // What the layout asked of its window, read when it opens. The pin changes only this
        // window, never the file.
        bool m_toolbar{ false };
        bool m_seeThrough{ false };
        bool m_keepOnTop{ false };

        // A drag on the toolbar's handle. Worked out in screen pixels, so a finger and a pen move
        // the window as well as a mouse does.
        bool m_handlePressed{ false };
        bool m_handleDragging{ false };
        uint32_t m_handlePointerId{ 0 };
        double m_handleStartX{ 0.0 };
        double m_handleStartY{ 0.0 };
        winrt::Windows::Graphics::PointInt32 m_handleWindowStart{};
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct RuntimeWindow : RuntimeWindowT<RuntimeWindow, implementation::RuntimeWindow>
    {
    };
}
