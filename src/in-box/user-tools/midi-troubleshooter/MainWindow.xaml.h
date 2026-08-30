// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "DriverTools.h"
#include "RegistryRepair.h"
#include "ReproLog.h"
#include "ServiceControl.h"
#include "SystemInfo.h"
#include "TroubleshooterItems.h"
#include "WindowChrome.h"

namespace winrt::miditroubleshooter::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        // runs before Activate, so it cannot touch the chrome instance
        void RestoreWindowPlacement();

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);

        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAppearanceButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnNavigationSelectionChanged(
            controls::NavigationView const& sender,
            controls::NavigationViewSelectionChangedEventArgs const& args);

        void OnRestartElevatedClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // API mode
        void OnApiModeSelectionChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnApplyApiModeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Diagnostics
        winrt::fire_and_forget OnRunMidiDiagClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRunMidiKsInfoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyMidiDiagClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyMidiKsInfoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnSaveMidiDiagClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnSaveMidiKsInfoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Capture
        winrt::fire_and_forget OnStartCaptureClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnStopCaptureClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnCancelCaptureClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Service health
        winrt::fire_and_forget OnRefreshServiceHealthClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRestartServiceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnSetAutomaticStartClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnSetManualStartClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Registry
        winrt::fire_and_forget OnRefreshRegistryClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRepairRegistryClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Drivers
        winrt::fire_and_forget OnRefreshDriversClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnUseUmpDriverClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnUseClassicDriverClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRemoveKorgDriverClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRemoveKorgBleDriverClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        // Everything the service knows, gathered off the UI thread in one pass.
        struct ServiceSnapshot
        {
            bool Gathered{ false };

            ::miditroubleshooter::ServiceStatus Service{};

            collections::IVectorView<midi2rept::MidiServiceSessionInfo> Sessions{ nullptr };
            collections::IVectorView<midi2rept::MidiServiceTransportPluginInfo> LoadedTransports{ nullptr };

            // what the registry says should exist, so a transport that failed to load still
            // shows up rather than silently vanishing
            std::vector<::miditroubleshooter::TransportRegistrationInfo> RegisteredTransports{};
        };

        void ShowPage(uint32_t const pageIndex) noexcept;
        controls::NavigationViewItem NavigationItemForPage(uint32_t const pageIndex) noexcept;
        uint32_t PageIndexForTag(winrt::hstring const& tag) noexcept;

        void ApplySystemInformation() noexcept;
        void ApplyElevationState() noexcept;

        void StartRefreshTimer() noexcept;
        void StopRefreshTimer() noexcept;

        // true when a page that polls the service is showing
        bool ServiceHealthPageVisible() const noexcept;

        winrt::fire_and_forget RequestServiceRefreshAsync() noexcept;
        static ServiceSnapshot GatherServiceSnapshot() noexcept;

        void ApplyServiceSnapshot(ServiceSnapshot const& snapshot) noexcept;
        void ApplySessions(ServiceSnapshot const& snapshot) noexcept;
        void ApplyTransports(ServiceSnapshot const& snapshot) noexcept;
        void ApplyServiceStatus(ServiceSnapshot const& snapshot) noexcept;

        void ApplyRegistryScan(::miditroubleshooter::RegistryScan const& scan) noexcept;
        void ApplyDriverDevices(
            std::vector<::miditroubleshooter::HardwareDeviceInfo> const& devices,
            std::vector<::miditroubleshooter::DriverPackageInfo> const& korgUsbPackages,
            std::vector<::miditroubleshooter::DriverPackageInfo> const& korgBlePackages) noexcept;

        foundation::IAsyncAction RemoveKorgPackagesAsync(::miditroubleshooter::KorgDriverKind const kind);

        void LoadApiMode() noexcept;

        // yes / cancel confirmation, used before anything that changes the machine
        foundation::IAsyncOperation<bool> ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message);

        // Offers a restart rather than taking one. Nothing here reboots without an answer.
        foundation::IAsyncAction OfferRestartAsync(winrt::hstring const& message);

        // False when the action needs administrator rights the app does not have. It has
        // already told the customer by the time it returns.
        bool RequireElevation() noexcept;

        winrt::fire_and_forget SaveTextAsync(winrt::hstring const& suggestedName, winrt::hstring const& text) noexcept;

        void CopyToClipboard(winrt::hstring const& text) noexcept;

        void AppendCaptureLog(std::vector<std::wstring> const& lines) noexcept;
        void SetCaptureButtonsForState(bool const running) noexcept;

        HWND WindowHandle() noexcept;

        midiapp::WindowChrome m_chrome{};

        // only one dialog can be open at a time, and a second ShowAsync throws
        controls::ContentDialog m_openDialog{ nullptr };

        collections::IObservableVector<miditroubleshooter::SessionItem> m_sessions{
            winrt::single_threaded_observable_vector<miditroubleshooter::SessionItem>() };

        collections::IObservableVector<miditroubleshooter::TransportItem> m_transports{
            winrt::single_threaded_observable_vector<miditroubleshooter::TransportItem>() };

        collections::IObservableVector<miditroubleshooter::RegistryEntryItem> m_drivers32Entries{
            winrt::single_threaded_observable_vector<miditroubleshooter::RegistryEntryItem>() };

        collections::IObservableVector<miditroubleshooter::RegistryEntryItem> m_drivers32WowEntries{
            winrt::single_threaded_observable_vector<miditroubleshooter::RegistryEntryItem>() };

        collections::IObservableVector<miditroubleshooter::RegistryEntryItem> m_serviceRootEntries{
            winrt::single_threaded_observable_vector<miditroubleshooter::RegistryEntryItem>() };

        collections::IObservableVector<miditroubleshooter::RegistryEntryItem> m_transportRegistryEntries{
            winrt::single_threaded_observable_vector<miditroubleshooter::RegistryEntryItem>() };

        collections::IObservableVector<miditroubleshooter::DriverDeviceItem> m_driverDevices{
            winrt::single_threaded_observable_vector<miditroubleshooter::DriverDeviceItem>() };

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };

        ::miditroubleshooter::SystemInformation m_systemInformation{};

        // the repair the last scan proposed, so the button applies exactly what was shown
        ::miditroubleshooter::RegistryScan m_registryScan{};

        ::miditroubleshooter::ReproCapture m_capture{};
        std::vector<::miditroubleshooter::DriverPackageInfo> m_korgUsbPackages{};
        std::vector<::miditroubleshooter::DriverPackageInfo> m_korgBlePackages{};

        winrt::hstring m_midiDiagOutput{};
        winrt::hstring m_midiKsInfoOutput{};

        uint32_t m_currentPageIndex{ 0 };

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_elevated{ false };

        // suppresses the Checked handler while the radio buttons are being set from the registry
        bool m_settingApiModeSelection{ false };

        // a tick is skipped rather than queued when the previous refresh is still running
        std::atomic<bool> m_refreshInFlight{ false };

        std::atomic<bool> m_busy{ false };
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
