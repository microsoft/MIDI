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

        void OnRefreshRemoteHostsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnConnectRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRetryRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
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

        void ShowRemotePage(bool const showRemote) noexcept;
        void UpdateManualConnectButton() noexcept;
        void UpdateCreateHostButtonState() noexcept;

        void SetRemoteStatus(winrt::hstring const& text) noexcept;
        void SetLocalStatus(winrt::hstring const& text) noexcept;

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
            winrt::hstring const customEndpointName);

        foundation::IAsyncOperation<bool> PromptForConnectNameAsync(
            winrt::hstring const deviceName,
            std::shared_ptr<winrt::hstring> customName);

        static winrt::hstring DescribeLatency(uint64_t const ticks) noexcept;
        static winrt::hstring JoinAddresses(collections::IVectorView<winrt::hstring> const& addresses) noexcept;

        midiapp::WindowChrome m_chrome{};

        controls::ContentDialog m_openDialog{ nullptr };

        midi2net::MidiNetworkAdvertisedHostWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };

        collections::IObservableVector<midinetworksetup::PendingInvitationItem> m_pendingInvitations{
            winrt::single_threaded_observable_vector<midinetworksetup::PendingInvitationItem>() };

        collections::IObservableVector<midinetworksetup::RemoteHostItem> m_remoteHosts{
            winrt::single_threaded_observable_vector<midinetworksetup::RemoteHostItem>() };

        collections::IObservableVector<midinetworksetup::LocalHostItem> m_localHosts{
            winrt::single_threaded_observable_vector<midinetworksetup::LocalHostItem>() };

        std::atomic<bool> m_refreshInProgress{ false };

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_transportMissingReported{ false };
    };
}

namespace winrt::midinetworksetup::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
