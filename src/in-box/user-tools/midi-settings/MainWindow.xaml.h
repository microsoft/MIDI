// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "MidiConfig.h"
#include "SettingsItems.h"
#include "ToolLauncher.h"
#include "WindowChrome.h"

namespace winrt::midisettings::implementation
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

        // Toolbar
        void OnLoopbackSetupClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnBluetoothSetupClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnNetworkSetupClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSysExClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnScratchPadClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnKeyboardClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTroubleshooterClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnGlobalSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Endpoint list
        void OnTransportFilterChanged(
            foundation::IInspectable const& sender,
            controls::SelectionChangedEventArgs const& args);
        void OnCardViewClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnListViewClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnEndpointClick(foundation::IInspectable const& sender, controls::ItemClickEventArgs const& args);
        void OnEndpointMonitorClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnEndpointPanicClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Endpoint details
        winrt::fire_and_forget OnDetailCustomizeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnDetailMonitorClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDetailPanicClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnDetailCopyIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Customization
        void OnCustomizeBrowseImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCustomizeClearImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnCustomizePortNamesClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMidi1PortNamesApproachChanged(foundation::IInspectable const& sender, xaml::Controls::SelectionChangedEventArgs const& args);

        // Global settings
        winrt::fire_and_forget OnApplyConfigFileClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCreateConfigFileClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyConfigFileClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnPortNamingChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRestartServiceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRestartElevatedClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        HWND WindowHandle() noexcept;

        // --- toolbar ---
        void ApplyToolButtons() noexcept;
        void ApplyToolButton(
            controls::Button const& button,
            controls::Image const& icon,
            ::midisettings::MidiTool const tool) noexcept;

        // --- endpoints ---
        winrt::fire_and_forget StartWatchersAsync() noexcept;
        void StopWatchers() noexcept;

        // Polls the service so the app comes back on its own after a service restart, and so
        // an endpoint list that quietly went stale is rebuilt rather than left there.
        void StartHealthTimer() noexcept;
        void StopHealthTimer() noexcept;
        winrt::fire_and_forget CheckServiceHealthAsync() noexcept;

        void RefreshTransportChoices() noexcept;
        void RefreshEndpointList() noexcept;
        void ApplyViewMode() noexcept;

        winrt::hstring TransportDisplayName(winrt::hstring const& transportCode) noexcept;
        winrt::hstring BuildEndpointDetail(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept;
        winrt::hstring ResolveEndpointImage(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept;

        midi2enum::MidiEndpointDeviceInformation FindEndpoint(winrt::hstring const& endpointDeviceId) noexcept;

        // --- details and customization ---
        foundation::IAsyncAction ShowEndpointDetailAsync(winrt::hstring endpointDeviceId);
        void PopulateDetail(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept;
        void RefreshDetailPorts() noexcept;

        foundation::IAsyncAction ShowCustomizeDialogAsync(winrt::hstring endpointDeviceId);
        void UpdateCustomizeImagePreview() noexcept;

        foundation::IAsyncAction ShowMidi1PortNamesDialogAsync(winrt::hstring endpointDeviceId);
        void PopulateMidi1PortNames(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept;
        void UpdateMidi1PortNamesApproachCaption() noexcept;

        winrt::fire_and_forget SendPanicAsync(winrt::hstring endpointDeviceId) noexcept;

        // --- global settings ---
        void RefreshGlobalSettings() noexcept;
        void ShowFirstRunInvitation() noexcept;

        midiapp::WindowChrome m_chrome{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_healthTimer{ nullptr };

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_serviceAvailable{ false };
        bool m_healthCheckInFlight{ false };
        bool m_suppressFilterHandling{ false };
        bool m_suppressPortNamingHandling{ false };

        // The detail dialog asks to be reopened after a customization, and its status line is
        // only safe to touch while it is actually up.
        bool m_detailDialogOpen{ false };
        bool m_customizeRequested{ false };

        // Set when the customize dialog is hidden to make room for the port names dialog, so the
        // caller knows to bring customize back afterwards rather than returning to the details.
        bool m_portNamesRequested{ false };

        // What the customer had typed when they stepped out to the port names dialog. Without
        // this, coming back would re-read the endpoint and silently discard their edits.
        bool m_customizeEditsPending{ false };
        winrt::hstring m_pendingCustomizeName{};
        winrt::hstring m_pendingCustomizeDescription{};

        midi2enum::MidiEndpointDeviceWatcher m_endpointWatcher{ nullptr };
        winrt::event_token m_endpointAddedToken{};
        winrt::event_token m_endpointRemovedToken{};
        winrt::event_token m_endpointUpdatedToken{};

        midi2legacy::MidiLegacyPortDeviceWatcher m_portWatcher{ nullptr };
        winrt::event_token m_portAddedToken{};
        winrt::event_token m_portRemovedToken{};
        winrt::event_token m_portUpdatedToken{};

        collections::IObservableVector<midisettings::EndpointItem> m_endpointItems{
            winrt::single_threaded_observable_vector<midisettings::EndpointItem>() };

        collections::IObservableVector<midisettings::Midi1PortItem> m_sourcePortItems{
            winrt::single_threaded_observable_vector<midisettings::Midi1PortItem>() };

        collections::IObservableVector<midisettings::Midi1PortItem> m_destinationPortItems{
            winrt::single_threaded_observable_vector<midisettings::Midi1PortItem>() };

        collections::IObservableVector<foundation::IInspectable> m_transportChoices{
            winrt::single_threaded_observable_vector<foundation::IInspectable>() };

        collections::IObservableVector<foundation::IInspectable> m_configFileChoices{
            winrt::single_threaded_observable_vector<foundation::IInspectable>() };

        // Transport code to display name, from the service. Empty when it could not be asked.
        std::map<std::wstring, std::wstring> m_transportNames{};

        // The endpoint the detail and customization dialogs are working on.
        winrt::hstring m_detailEndpointId{};

        // Bare file name of the picture chosen in the customization dialog, already copied into
        // the shared assets folder. Empty means no picture.
        winrt::hstring m_customizeImageFileName{};

        winrt::hstring m_portNamesEndpointDeviceId{};

        collections::IObservableVector<midisettings::Midi1PortNameItem> m_midi1PortNameSources{
            winrt::single_threaded_observable_vector<midisettings::Midi1PortNameItem>() };
        collections::IObservableVector<midisettings::Midi1PortNameItem> m_midi1PortNameDestinations{
            winrt::single_threaded_observable_vector<midisettings::Midi1PortNameItem>() };

        // Kept alongside the rows so the resolved "current name" column can be recalculated when
        // the naming style changes, without re-reading the table from the service.
        std::vector<midi2enum::Midi1PortNameTableEntry> m_midi1PortNameSourceEntries{};
        std::vector<midi2enum::Midi1PortNameTableEntry> m_midi1PortNameDestinationEntries{};
    };
}

namespace winrt::midisettings::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
