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
#include "BindingEngine.h"
#include "ValueThrottle.h"
#include "DeviceCatalog.h"
#include "OutputRouter.h"
#include "SurfaceRenderer.h"
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

        void OnViewModeToggled(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnFullScreenClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnPanicClick(
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

    private:
        void OnWindowClosed(
            foundation::IInspectable const& sender,
            xaml::WindowEventArgs const& args);

        // ---- surface ----

        void BuildPage(_In_ size_t pageIndex);
        void ApplyScale();
        void UpdateDeckBrush();
        void SetFullScreen(_In_ bool fullScreen);
        void Panic();

        // ---- devices and sending ----

        void StartDevices();
        void ReopenConnections();
        void UpdateDeviceStatus();

        // Every send goes through here, on the UI thread, straight out of the pointer handler.
        void SendPrepared(_In_ uint32_t count) noexcept;

        void OnControlValueChanged(_In_ size_t itemIndex, _In_ double value, _In_ bool isFinal);
        void OnControlSetDirectly(_In_ size_t itemIndex, _In_ double value);
        void OnControlSwitched(_In_ size_t itemIndex, _In_ bool isOn);

        void SendStartupValues();
        void OnFeedbackWords(
            _In_ uint64_t timestamp,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* words);

        midiapp::WindowChrome m_chrome{};

        std::wstring m_filePath{};
        glass::LayoutDocument m_document{};
        glass::Theme m_theme{};

        size_t m_pageIndex{ 0 };

        glass::SurfaceRenderer m_renderer{};
        glass::InputRouter m_input{};
        glass::BindingEngine m_engine{};
        glass::DeviceCatalog m_devices{};

        // One throttle per control, so a fader on a DIN cable can be limited without touching a
        // note on. Indexed the same way the engine indexes controls.
        std::vector<glass::ValueThrottle> m_throttles{};

        // Rebuilt whenever the device table changes, and only ever read on the UI thread, so the
        // path a finger takes never waits on a lock.
        std::vector<winrt::com_ptr<IMidiEndpointConnectionRaw>> m_sendTable{};

        // Caller-owned, reused, never resized on the hot path.
        std::array<glass::PreparedSend, glass::MaximumSendsPerEvent> m_sends{};

        std::wstring m_ownerId{};

        // What the device table resolved to last time. An unrelated device arriving must not
        // rebuild connections that did not change, and must not replay the startup values.
        std::wstring m_destinationSignature{};

        // A layout initializes once per run. Replaying it because a webcam was plugged in would
        // push a whole desk back to its opening positions in the middle of a set.
        bool m_startupValuesSent{ false };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_updatingChrome{ false };
        bool m_fullScreen{ false };
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct RuntimeWindow : RuntimeWindowT<RuntimeWindow, implementation::RuntimeWindow>
    {
    };
}
