// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "NetworkItems.h"
#include "ConfigFile.h"

namespace winrt::midinetworksetup::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        // called from App::OnLaunched before Activate, so the window does not visibly jump
        void RestoreWindowPlacement();

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);

        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAppearanceButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnNavigationSelectionChanged(
            controls::NavigationView const& sender,
            controls::NavigationViewSelectionChangedEventArgs const& args);

        void OnTransportSettingChanged(
            controls::NumberBox const& sender,
            controls::NumberBoxValueChangedEventArgs const& args);

        void OnRestoreTransportSettingDefaultsClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);


        winrt::fire_and_forget OnConnectRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRetryRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnReassociateRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnReassociateSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        winrt::fire_and_forget OnCustomizeRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBrowseForImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRemoveImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyEndpointDeviceIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorHostConnectionClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDisconnectRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnManualConnectFieldChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnManualConnectPortChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        winrt::fire_and_forget OnManualConnectClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnAllowInvitationOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnAllowInvitationAlwaysClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDenyInvitationOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBlockInvitationClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnCreateHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCreateHostFieldChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnCreateHostPortModeChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnCreateHostPrimaryButtonClick(
            controls::ContentDialog const& sender,
            controls::ContentDialogButtonClickEventArgs const& args);

        winrt::fire_and_forget OnStartStopHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDeleteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnDisconnectRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBlockRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnForgetKnownClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        // everything the service knows, gathered off the UI thread in one pass
        struct ServiceSnapshot
        {
            bool TransportAvailable{ false };
            bool Gathered{ false };

            std::vector<midi2net::MidiNetworkAdvertisedHost> AdvertisedHosts{};
            collections::IVectorView<midi2net::MidiNetworkConfiguredClient> ConfiguredClients{ nullptr };
            collections::IVectorView<midi2net::MidiNetworkConfiguredHost> ConfiguredHosts{ nullptr };
            collections::IVectorView<midi2net::MidiNetworkPendingRemoteClient> PendingRemoteClients{ nullptr };

            std::unordered_map<std::wstring, winrt::hstring> ClientDisplayNames{};

            // lowercased entry identifiers of the clients the configuration file has
            std::vector<std::wstring> ConfiguredClientIds{};

            // allow and deny list entries, keyed by the host entry identifier
            std::unordered_map<std::wstring, std::vector<::midinetworksetup::KnownClientEntry>> KnownClients{};
        };

        void StartWatcher() noexcept;
        void StopWatcher() noexcept;

        void StartRefreshTimer() noexcept;
        void StopRefreshTimer() noexcept;

        winrt::fire_and_forget RequestRefreshAsync() noexcept;
        static ServiceSnapshot GatherSnapshot() noexcept;

        void ApplySnapshot(ServiceSnapshot const& snapshot) noexcept;
        void ApplyPendingInvitations(ServiceSnapshot const& snapshot) noexcept;
        void ApplyRemoteHosts(ServiceSnapshot const& snapshot) noexcept;
        void ApplyLocalHosts(ServiceSnapshot const& snapshot) noexcept;

        void ShowPage(uint32_t const pageIndex) noexcept;
        winrt::Windows::Foundation::IInspectable NavigationItemForPage(uint32_t const pageIndex) noexcept;

        // Reads what the transport is running with and puts it in the boxes. The boxes raise
        // ValueChanged when they are filled in, so m_loadingTransportSettings suppresses the
        // write-back that would otherwise follow.
        void LoadTransportSettings() noexcept;
        void QueueTransportSettingsWrite() noexcept;
        void FlushPendingTransportSettingsWrite() noexcept;
        winrt::fire_and_forget ApplyTransportSettingsAsync() noexcept;

        // The intervals are entered in milliseconds because that is the unit the service takes,
        // but a five or six digit number is hard to judge at a glance.
        void UpdateTransportSettingSecondsText() noexcept;

        // Appends the default to each setting's description, so the customer can see it without
        // having to reset the box to find out what it was.
        void AppendTransportSettingDefaults() noexcept;

        void UpdateManualConnectButton() noexcept;
        void UpdateCreateHostButtonState() noexcept;

        void SetRemoteStatus(winrt::hstring const& text) noexcept;
        void SetLocalStatus(winrt::hstring const& text) noexcept;

        // A status line describes something that just happened, so it stops being true within
        // seconds. Shows the text, then fades it away rather than leaving a stale claim on screen.
        void ShowTransientStatus(
            winrt::Microsoft::UI::Xaml::Controls::TextBlock const& target,
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer& timer,
            winrt::hstring const& text) noexcept;

        // False when the transport is missing or older than the verbs this app needs. It has
        // already shown the customer why by the time it returns.
        bool VerifyTransportIsUsable() noexcept;

        // yes / cancel confirmation, used before anything destructive
        foundation::IAsyncOperation<bool> ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message);

        winrt::fire_and_forget AnswerInvitationAsync(            midinetworksetup::PendingInvitationItem const item,
            bool const approve,
            bool const thisRequestOnly);

        winrt::fire_and_forget AnswerRemoteClientAsync(
            midinetworksetup::HostConnectionItem const item,
            bool const approve,
            bool const thisRequestOnly);

        winrt::fire_and_forget DisconnectRemoteClientAsync(
            midinetworksetup::HostConnectionItem const item);

        winrt::fire_and_forget ConnectRemoteHostAsync(
            midinetworksetup::RemoteHostItem const item,
            bool const reuseExistingEntry,
            winrt::hstring const customEndpointName,
            winrt::guid const explicitClientId = winrt::guid{});

        foundation::IAsyncOperation<bool> PromptForConnectNameAsync(
            winrt::hstring const deviceName,
            std::shared_ptr<winrt::hstring> customName);

        // Picks which advertised device a saved entry should point at instead. Deliberately a
        // choice rather than an automatic match: guessing which device replaced another has
        // caused trouble before, and the customer knows which unit they updated.
        foundation::IAsyncOperation<bool> PromptForReassociateTargetAsync(
            winrt::hstring const entryName,
            std::shared_ptr<midinetworksetup::RemoteHostItem> chosen);

        // Shows the customization for an endpoint which already exists and saves the result. The
        // MIDI 1.0 ports choice is handled separately, because it is decided when the endpoint is
        // built rather than written to it.
        foundation::IAsyncOperation<bool> ShowCustomizeDialogAsync(
            midinetworksetup::RemoteHostItem const item,
            std::shared_ptr<winrt::hstring> errorMessage);

        static winrt::hstring DescribeLatency(uint64_t const ticks) noexcept;
        static winrt::hstring JoinAddresses(collections::IVectorView<winrt::hstring> const& addresses) noexcept;

        // A host listens on every interface, so the service reports the wildcard address. That is
        // accurate, and useless to somebody who has to type an address into a synth, so this
        // supplies the addresses of this PC instead.
        static winrt::hstring DisplayAddressForLocalHost(winrt::hstring const& actualAddress) noexcept;

        midiapp::WindowChrome m_chrome{};

        controls::ContentDialog m_openDialog{ nullptr };

        midi2net::MidiNetworkAdvertisedHostWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_remoteStatusTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_localStatusTimer{ nullptr };

        // Holding a NumberBox spinner raises ValueChanged on every step, and each one would
        // otherwise be a round trip to the service and a rewrite of the configuration file.
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_transportSettingsWriteTimer{ nullptr };

        collections::IObservableVector<midinetworksetup::PendingInvitationItem> m_pendingInvitations{
            winrt::single_threaded_observable_vector<midinetworksetup::PendingInvitationItem>() };

        collections::IObservableVector<midinetworksetup::RemoteHostItem> m_remoteHosts{
            winrt::single_threaded_observable_vector<midinetworksetup::RemoteHostItem>() };

        collections::IObservableVector<midinetworksetup::LocalHostItem> m_localHosts{
            winrt::single_threaded_observable_vector<midinetworksetup::LocalHostItem>() };

        std::atomic<bool> m_refreshInProgress{ false };

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_loadingTransportSettings{ false };

        // The defaults are appended to the description text once, so a later reload does not
        // append them a second time.
        bool m_transportSettingDefaultsShown{ false };
        bool m_transportMissingReported{ false };
    };
}

namespace winrt::midinetworksetup::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
