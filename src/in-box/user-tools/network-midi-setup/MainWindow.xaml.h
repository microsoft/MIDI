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
#include "ServiceFirewall.h"

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

        void OnNotificationsBarButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnNotificationsBarCloseClick(controls::InfoBar const& sender, foundation::IInspectable const& args);

        void OnNavigationSelectionChanged(
            controls::NavigationView const& sender,
            controls::NavigationViewSelectionChangedEventArgs const& args);

        void OnTransportSettingChanged(
            controls::NumberBox const& sender,
            controls::NumberBoxValueChangedEventArgs const& args);

        void OnRestoreTransportSettingDefaultsClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnFirewallNetworkChoiceChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnAllowThroughFirewallClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);


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
        winrt::fire_and_forget OnChangeRemoteHostSendSpeedClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        // The MIDI 1.0 ports a Network MIDI 2.0 connection gets when the device doesn't describe
        // its own groups. See MainWindowPortGroups.cpp.
        void OnStepAsideForMidi1PortsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnMidi1PortGroupsPrimaryButtonClick(_In_ controls::ContentDialog const& sender, _In_ controls::ContentDialogButtonClickEventArgs const& args);
        void OnMidi1PortGroupsUseHostChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnManualConnectMidi1PortsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnChangeRemoteHostMidi1PortsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnChangeHostMidi1PortsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnChangeRemoteClientMidi1PortsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnChangeRemoteClientMidi1PortsEntryClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnManualConnectFieldChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnManualConnectPortChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        winrt::fire_and_forget OnManualConnectClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // both pages' Connect by address forms
        void OnManualConnectExpanding(controls::Expander const& sender, controls::ExpanderExpandingEventArgs const& args);
        void OnManualConnectFormSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);

        winrt::fire_and_forget OnAllowInvitationOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnAllowInvitationAlwaysClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDenyInvitationOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBlockInvitationClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnCreateHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCreateHostFieldChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnCreateHostPortModeChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // every network adapter picker: falling back only means something once one is chosen
        void OnNetworkAdapterChoiceChanged(
            _In_ foundation::IInspectable const& sender,
            _In_ controls::SelectionChangedEventArgs const& args);

        // By value, not by reference: this is a coroutine, and a reference parameter is not kept
        // in the coroutine frame, so it dangles once the first co_await returns control to XAML.
        winrt::fire_and_forget OnCreateHostPrimaryButtonClick(
            controls::ContentDialog sender,
            controls::ContentDialogButtonClickEventArgs args);

        winrt::fire_and_forget OnStartStopHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDeleteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnChangeHostAdapterClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnChangeHostSendSpeedClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        // One device's speed on a host, for both transports: from a connection row, and from the
        // host's list of devices with a speed of their own
        void OnChangeRemoteClientSendSpeedClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnChangeRemoteClientSpeedEntryClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnUseHostSpeedForRemoteClientClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        // the speed dialog's "use the host's speed" check box
        void OnChangeSendSpeedUseHostChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnDisconnectRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBlockRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnForgetKnownClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // RTP-MIDI pages
        winrt::fire_and_forget OnConnectRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRetryRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDisconnectRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnCustomizeRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnChangeRtpRemoteHostSendSpeedClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRtpOfflineRetentionChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        winrt::fire_and_forget ApplyRtpOfflineRetentionChoiceAsync(_In_ controls::ComboBox const combo);
        void OnCopyRtpEndpointDeviceIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorRtpRemoteHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorRtpHostConnectionClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnRtpManualConnectFieldChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnRtpManualConnectPortChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        winrt::fire_and_forget OnRtpManualConnectClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnCreateRtpHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCreateRtpHostFieldChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnCreateRtpHostPortModeChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnStartStopRtpHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDeleteRtpHostClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnChangeRtpHostAdapterClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnChangeRtpHostSendSpeedClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnDisconnectRtpRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBlockRtpRemoteClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnForgetRtpKnownClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // A send speed limit as the customer reads it. 0 is no limit.
        static winrt::hstring SendSpeedLimitText(_In_ uint32_t const multiple) noexcept;

        // The same, with whether it slows down by itself when a device misses data, and whether it
        // skips in-between values to catch up
        static winrt::hstring SendSpeedValueText(
            _In_ uint32_t const multiple,
            _In_ bool const reduceAutomatically,
            _In_ bool const skipInBetweenValues) noexcept;

    private:
        // What the customer chose in the connect dialog. The name is empty when they want the
        // name the device reports.
        struct ConnectChoices
        {
            winrt::hstring CustomName{};
            uint32_t SendSpeedLimit{ 0 };
            bool ReduceSendSpeedAutomatically{ false };
            bool SkipInBetweenValuesToCatchUp{ true };

            // RTP-MIDI only
            bool SendRecoveryJournal{ true };
            int32_t OfflineRetentionSeconds{ 0 };

            // Network MIDI 2.0 only
            Midi1PortLayout Midi1Ports{};
        };

        // everything the service knows, gathered off the UI thread in one pass
        struct ServiceSnapshot
        {
            bool TransportAvailable{ false };
            bool Gathered{ false };

            std::vector<midi2net::MidiNetworkAdvertisedHost> AdvertisedHosts{};
            collections::IVectorView<midi2net::MidiNetworkConfiguredClient> ConfiguredClients{ nullptr };
            collections::IVectorView<midi2net::MidiNetworkConfiguredHost> ConfiguredHosts{ nullptr };
            collections::IVectorView<midi2net::MidiNetworkPendingRemoteClient> PendingRemoteClients{ nullptr };

            // the clients saved in the configuration, keyed by the entry identifier
            std::unordered_map<std::wstring, midi2net::MidiNetworkSavedClient> SavedClients{};

            // the same for hosts, for what each one gives a device by default
            std::unordered_map<std::wstring, midi2net::MidiNetworkSavedHost> SavedHosts{};

            // Lowercased ids of the Network MIDI 2.0 endpoints whose device describes its own
            // groups, which then decide its MIDI 1.0 ports. Each one is a device property read, so
            // it's done here, off the UI thread.
            std::vector<std::wstring> EndpointsWithFunctionBlocks{};

            // saved allow and deny decisions, keyed by the host entry identifier
            std::unordered_map<std::wstring, std::vector<midi2net::MidiNetworkKnownRemoteClient>> KnownClients{};

            // RTP-MIDI keeps everything, remembered decisions included, in the service, so none of
            // it is read from the configuration file
            bool RtpAvailable{ false };
            bool RtpGathered{ false };
            collections::IVectorView<midi2rtp::MidiRtpAdvertisedHost> RtpAdvertisedHosts{ nullptr };
            collections::IVectorView<midi2rtp::MidiRtpConfiguredClient> RtpConfiguredClients{ nullptr };
            collections::IVectorView<midi2rtp::MidiRtpConfiguredHost> RtpConfiguredHosts{ nullptr };
            collections::IVectorView<midi2rtp::MidiRtpPendingRemoteClient> RtpPendingRemoteClients{ nullptr };
        };

        void StartWatcher() noexcept;
        void StopWatcher() noexcept;

        void StartRefreshTimer() noexcept;
        void StopRefreshTimer() noexcept;

        winrt::fire_and_forget RequestRefreshAsync() noexcept;
        static ServiceSnapshot GatherSnapshot(_In_ bool const includeNetworkMidi2, _In_ bool const includeRtp) noexcept;

        void ApplySnapshot(ServiceSnapshot const& snapshot) noexcept;
        void ApplyPendingInvitations(ServiceSnapshot const& snapshot) noexcept;
        void ApplyRemoteHosts(ServiceSnapshot const& snapshot) noexcept;
        void ApplyLocalHosts(ServiceSnapshot const& snapshot) noexcept;
        void ApplyRtpRemoteHosts(ServiceSnapshot const& snapshot) noexcept;
        void ApplyRtpLocalHosts(ServiceSnapshot const& snapshot) noexcept;

        // Lowercased host names and addresses of every Network MIDI 2.0 device in the snapshot. An
        // RTP-MIDI session on one of those machines is flagged, because Network MIDI 2.0 is the
        // better way to reach it.
        static std::vector<std::wstring> CollectNetworkMidi2Identities(_In_ ServiceSnapshot const& snapshot) noexcept;
        bool IsNetworkMidi2Machine(
            _In_ winrt::hstring const& hostName,
            _In_ collections::IVectorView<winrt::hstring> const& addresses) const noexcept;
        bool IsNetworkMidi2Machine(_In_ winrt::hstring const& hostNameOrAddress) const noexcept;

        // Whether anything is watching for connection requests while this window is closed. Only
        // reports: the notifications app is started by the customer, never by this app.
        void RefreshNotificationsBanner() noexcept;

        // The layout for the window's size: below a width, the navigation pane folds into a menu
        // button and the rows drop their graphs and addresses. Also keeps the tall dialogs short
        // enough to stay clear of the title bar.
        void ApplyWindowSize(_In_ foundation::Size const size) noexcept;

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

        // The firewall page reads Windows Firewall when it is shown, when the window comes back to
        // the front, and after a change, rather than on the refresh timer: it walks every rule.
        winrt::fire_and_forget RefreshFirewallStateAsync() noexcept;
        void ApplyFirewallState(_In_ ::midinetworksetup::firewall::FirewallState const& state) noexcept;
        void UpdateFirewallControls() noexcept;

        void UpdateManualConnectButton() noexcept;
        void UpdateCreateHostButtonState() noexcept;
        void UpdateRtpManualConnectButton() noexcept;
        void UpdateCreateRtpHostButtonState() noexcept;

        // fills the speed pickers which are on the pages rather than in a dialog
        void InitializeSendSpeedPickers() noexcept;

        void SetRemoteStatus(winrt::hstring const& text) noexcept;
        void SetLocalStatus(winrt::hstring const& text) noexcept;
        void SetRtpRemoteStatus(winrt::hstring const& text) noexcept;
        void SetRtpLocalStatus(winrt::hstring const& text) noexcept;

        // A status line describes something that just happened, so it stops being true within
        // seconds. Shows the text, then fades it away rather than leaving a stale claim on screen.
        void ShowTransientStatus(
            winrt::Microsoft::UI::Xaml::Controls::TextBlock const& target,
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer& timer,
            winrt::hstring const& text) noexcept;

        // False when neither transport is usable. Network MIDI 2.0 has to be installed and new
        // enough for the verbs this app needs; RTP-MIDI pages are only offered when it is
        // installed at all. It has already shown the customer why by the time it returns.
        bool VerifyTransportIsUsable() noexcept;

        // Network MIDI 2.0 checks the capabilities it needs, so an older transport is reported
        // rather than failing one operation at a time.
        static bool IsNetworkMidi2TransportUsable() noexcept;
        static bool IsRtpTransportUsable() noexcept;

        // yes / cancel confirmation, used before anything destructive
        foundation::IAsyncOperation<bool> ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message);

        // Asks which network adapter a host should use. True when the customer saved a change,
        // which is then read from the dialog's controls. By value, because this is a coroutine.
        foundation::IAsyncOperation<bool> ShowChangeHostAdapterDialogAsync(_In_ midinetworksetup::LocalHostItem const item);

        // Asks how fast a host or a device should be sent to. True when the customer saved a
        // change, which is then read from the dialog's controls. RTP-MIDI cannot tell that a device
        // missed data, so it does not offer to slow down by itself. For one device on a host, a
        // non-empty hostSendSpeedText also offers going back to the host's speed.
        foundation::IAsyncOperation<bool> ShowChangeSendSpeedDialogAsync(
            _In_ winrt::hstring const name,
            _In_ uint32_t const currentSendSpeedLimit,
            _In_ bool const currentReduceAutomatically,
            _In_ bool const currentSkipInBetweenValues,
            _In_ bool const offerReduceAutomatically,
            _In_ winrt::hstring const hostSendSpeedText,
            _In_ bool const currentUsesHostSpeed,
            _In_ bool const offerOfflineRetention = false,
            _In_ int32_t const currentOfflineRetentionSeconds = 0);

        // The offline retention the dialog above was left at
        int32_t ChosenOfflineRetentionSeconds(_In_ int32_t const currentOfflineRetentionSeconds) noexcept;

        // Asks for one device's speed on a host, then applies it. hasOwnSpeed says whether the
        // device already has a speed of its own, which the own values then describe.
        winrt::fire_and_forget EditRemoteClientSendSpeedAsync(
            _In_ winrt::hstring const hostKey,
            _In_ winrt::hstring const remoteName,
            _In_ winrt::hstring const remoteProductInstanceId,
            _In_ bool const isRtpMidi,
            _In_ bool const hasOwnSpeed,
            _In_ uint32_t const ownSendSpeedLimit,
            _In_ bool const ownReduceAutomatically,
            _In_ bool const ownSkipInBetweenValues);

        // Gives one device on a host a speed of its own, or puts it back on the host's speed. The
        // host's whole list is sent and saved, so it is rebuilt from what the service holds now
        // with only this device changed.
        winrt::fire_and_forget ChangeRemoteClientSendSpeedAsync(
            _In_ winrt::hstring const hostKey,
            _In_ winrt::hstring const remoteName,
            _In_ winrt::hstring const remoteProductInstanceId,
            _In_ bool const isRtpMidi,
            _In_ bool const useHostSpeed,
            _In_ uint32_t const sendSpeedLimit,
            _In_ bool const reduceAutomatically,
            _In_ bool const skipInBetweenValues,
            _In_ int32_t const offlineRetentionSeconds = 0);

        // RTP-MIDI has no verb for changing a host, so this creates it again with the same id and
        // the given adapter and speed, copying everything else from what was saved.
        winrt::fire_and_forget ReplaceRtpHostAsync(
            _In_ midinetworksetup::LocalHostItem const item,
            _In_ winrt::guid const hostId,
            _In_ winrt::guid const adapterId,
            _In_ bool const allowAdapterFallback,
            _In_ uint32_t const sendSpeedLimit,
            _In_ bool const skipInBetweenValuesToCatchUp,
            _In_ winrt::hstring const doneMessage);

        winrt::fire_and_forget AnswerInvitationAsync(            midinetworksetup::PendingInvitationItem const item,
            bool const approve,
            bool const thisRequestOnly);

        winrt::fire_and_forget AnswerRemoteClientAsync(
            midinetworksetup::HostConnectionItem const item,
            bool const approve,
            bool const thisRequestOnly);

        winrt::fire_and_forget DisconnectRemoteClientAsync(
            midinetworksetup::HostConnectionItem const item);

        // The speed and the MIDI 1.0 ports are only used for a new entry. Trying a saved entry
        // again keeps what it was saved with.
        winrt::fire_and_forget ConnectRemoteHostAsync(
            _In_ midinetworksetup::RemoteHostItem const item,
            _In_ bool const reuseExistingEntry,
            _In_ winrt::hstring const customEndpointName,
            _In_ uint32_t const sendSpeedLimit,
            _In_ bool const reduceSendSpeedAutomatically,
            _In_ bool const skipInBetweenValuesToCatchUp,
            _In_ bool const createMidi1Ports,
            _In_ Midi1PortLayout const midi1Ports,
            _In_ winrt::guid const explicitClientId = winrt::guid{});

        // Asks for an optional display name and the sending speed before connecting. RTP-MIDI
        // cannot tell that a device missed data, so it is not offered the slow down choice. It is
        // offered the recovery journal choice, which Network MIDI 2.0 does not have.
        foundation::IAsyncOperation<bool> PromptForConnectNameAsync(
            _In_ winrt::hstring const deviceName,
            _In_ bool const offerReduceAutomatically,
            _In_ std::shared_ptr<ConnectChoices> choices);

        // Picks which advertised device a saved entry should point at instead. Deliberately a
        // choice rather than an automatic match: guessing which device replaced another has
        // caused trouble before, and the customer knows which unit they updated.
        foundation::IAsyncOperation<bool> PromptForReassociateTargetAsync(
            winrt::hstring const entryName,
            std::shared_ptr<midinetworksetup::RemoteHostItem> chosen);

        // Shows the customization for an endpoint which already exists and saves the result. The
        // MIDI 1.0 ports choice is handled separately, because it is decided when the endpoint is
        // built rather than written to it. RTP-MIDI endpoints always have their one MIDI 1.0 port,
        // so for them that part of the dialog is hidden.
        foundation::IAsyncOperation<bool> ShowCustomizeDialogAsync(
            winrt::hstring const endpointDeviceId,
            winrt::hstring const clientKey,
            bool const isRtpMidi,
            std::shared_ptr<winrt::hstring> errorMessage);

        // ---- MIDI 1.0 ports for Network MIDI 2.0, in MainWindowPortGroups.cpp ----

        // What the MIDI 1.0 ports dialog is changing, which decides what it offers
        enum class Midi1PortsScope
        {
            // A connection from this PC, or one device on a host. Ports can be named.
            Device,

            // What a host gives every device which connects to it. No names, because every device
            // would get the same ones.
            Host,
        };

        // Shows the MIDI 1.0 ports dialog. True when the customer pressed the primary button, with
        // their choice in layout. A non-empty hostPortsText offers one device on a host the host's
        // ports instead, and useHostPorts says whether it starts on them and whether it ended on
        // them. By value, because this is a coroutine.
        foundation::IAsyncOperation<bool> ShowMidi1PortGroupsDialogAsync(
            _In_ winrt::hstring const title,
            _In_ Midi1PortsScope const scope,
            _In_ winrt::hstring const primaryButtonText,
            _In_ winrt::hstring const hostPortsText,
            _In_ std::shared_ptr<Midi1PortLayout> layout,
            _In_ std::shared_ptr<bool> useHostPorts);

        void FillMidi1PortGroupRows(_In_ Midi1PortLayout const& layout, _In_ bool const offerNames) noexcept;
        Midi1PortLayout ReadMidi1PortGroupRows() const noexcept;

        // Why the groups on screen can't be used, or empty when they can
        winrt::hstring Midi1PortGroupRowsProblem() noexcept;

        // Shows a problem in place of the hint, or the hint again when the problem is empty
        void ShowMidi1PortGroupsProblem(_In_ winrt::hstring const& problem) noexcept;

        // Asks for one device's MIDI 1.0 ports on a host, then applies them. hasOwnPorts says
        // whether it already has ports of its own, which ownPorts then describes.
        winrt::fire_and_forget EditRemoteClientMidi1PortsAsync(
            _In_ winrt::hstring const hostKey,
            _In_ winrt::hstring const remoteName,
            _In_ winrt::hstring const remoteProductInstanceId,
            _In_ bool const hasOwnPorts,
            _In_ Midi1PortLayout const ownPorts);

        // Gives one device on a host MIDI 1.0 ports of its own, or puts it back on the host's. As
        // with a speed change, the host's whole list is rebuilt from what the service holds now,
        // with only this device changed, and its speed is kept.
        winrt::fire_and_forget ChangeRemoteClientMidi1PortsAsync(
            _In_ winrt::hstring const hostKey,
            _In_ winrt::hstring const remoteName,
            _In_ winrt::hstring const remoteProductInstanceId,
            _In_ bool const useHostPorts,
            _In_ Midi1PortLayout const ports);

        // the dialog's rows
        collections::IObservableVector<midinetworksetup::Midi1PortGroupItem> m_midi1PortGroupRows{
            winrt::single_threaded_observable_vector<midinetworksetup::Midi1PortGroupItem>() };

        // what the next connection made by address gets
        Midi1PortLayout m_manualConnectMidi1Ports{};

        // Set when an open dialog steps aside for the MIDI 1.0 ports dialog, so the code waiting on
        // it shows the ports and then brings it back, rather than taking it as canceled
        bool m_midi1PortGroupsRequested{ false };

        // The installed transport can create and name MIDI 1.0 ports by group. Decided once at
        // startup, with m_networkMidi2Usable. An older transport only takes a count.
        bool m_midi1PortGroupsUsable{ false };

        winrt::fire_and_forget ConnectRtpRemoteHostAsync(
            _In_ midinetworksetup::RtpRemoteHostItem const item,
            _In_ winrt::hstring const customEndpointName,
            _In_ uint32_t const sendSpeedLimit,
            _In_ bool const skipInBetweenValuesToCatchUp,
            _In_ bool const sendRecoveryJournal,
            _In_ int32_t const offlineRetentionSeconds);

        // Network MIDI 2.0 is the better choice when a machine offers both, so connecting over
        // RTP-MIDI to one of those asks first. It never refuses: the customer decides.
        foundation::IAsyncOperation<bool> ConfirmRtpAlongsideNetworkMidi2Async(winrt::hstring const deviceName);

        // The service keeps an always or never decision only until it restarts. This writes the
        // host's lasting decisions, as the service now holds them, to the configuration file.
        static bool SaveRtpKnownClients(_In_ winrt::guid const& hostId, _Out_ winrt::hstring& errorMessage) noexcept;

        // Which RTP-MIDI error an entry last hit, as something a person can act on
        static winrt::hstring DescribeRtpClientProblem(_In_ int32_t const lastErrorCode) noexcept;

        // Why a Network MIDI 2.0 entry's last invitation did not open a session. Empty when it did.
        static winrt::hstring DescribeNetworkClientProblem(_In_ midi2net::MidiNetworkClientConnectErrorCode const lastErrorCode) noexcept;

        static winrt::hstring DescribeLatency(uint64_t const ticks) noexcept;
        static winrt::hstring JoinAddresses(collections::IVectorView<winrt::hstring> const& addresses) noexcept;

        // A host listens on every interface, so the service reports the wildcard address. That is
        // accurate, and useless to somebody who has to type an address into a synth, so this
        // supplies the addresses of this PC instead.
        static winrt::hstring DisplayAddressForLocalHost(winrt::hstring const& actualAddress) noexcept;

        midiapp::WindowChrome m_chrome{};

        controls::ContentDialog m_openDialog{ nullptr };

        // A Connect by address form that's opening, until it has its full height
        controls::Expander m_openingConnectForm{ nullptr };

        midi2net::MidiNetworkAdvertisedHostWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_remoteStatusTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_localStatusTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_rtpRemoteStatusTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_rtpLocalStatusTimer{ nullptr };

        // Holding a NumberBox spinner raises ValueChanged on every step, and each one would
        // otherwise be a round trip to the service and a rewrite of the configuration file.
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_transportSettingsWriteTimer{ nullptr };

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_firewallStatusTimer{ nullptr };

        collections::IObservableVector<midinetworksetup::PendingInvitationItem> m_pendingInvitations{
            winrt::single_threaded_observable_vector<midinetworksetup::PendingInvitationItem>() };

        collections::IObservableVector<midinetworksetup::RemoteHostItem> m_remoteHosts{
            winrt::single_threaded_observable_vector<midinetworksetup::RemoteHostItem>() };

        collections::IObservableVector<midinetworksetup::LocalHostItem> m_localHosts{
            winrt::single_threaded_observable_vector<midinetworksetup::LocalHostItem>() };

        collections::IObservableVector<midinetworksetup::RtpRemoteHostItem> m_rtpRemoteHosts{
            winrt::single_threaded_observable_vector<midinetworksetup::RtpRemoteHostItem>() };

        collections::IObservableVector<midinetworksetup::LocalHostItem> m_rtpLocalHosts{
            winrt::single_threaded_observable_vector<midinetworksetup::LocalHostItem>() };

        // decided once at startup, so the pages on offer do not change under the customer
        bool m_networkMidi2Usable{ false };
        bool m_rtpUsable{ false };
        bool m_rtpMissingReported{ false };

        // refreshed with every snapshot, from the UI thread only
        std::vector<std::wstring> m_networkMidi2Identities{};

        std::atomic<bool> m_refreshInProgress{ false };

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_loadingTransportSettings{ false };

        // The defaults are appended to the description text once, so a later reload does not
        // append them a second time.
        bool m_transportSettingDefaultsShown{ false };
        bool m_transportMissingReported{ false };

        // Closing the notifications bar means "not now", so it stays closed until the app has
        // run and stopped again rather than coming back on the next refresh.
        bool m_notificationsBannerDismissed{ false };

        // The firewall page. The network choices are set from the first reading only, and are the
        // customer's after that.
        bool m_firewallBusy{ false };
        bool m_firewallRefreshInProgress{ false };
        bool m_firewallServiceFound{ false };
        bool m_firewallChoicesSet{ false };

        // only a PC joined to a domain can be on a domain network, so only it gets that box
        bool m_firewallShowDomain{ false };

        // what this app's rule covers now, so the button is only offered for a change
        long m_firewallOwnRuleNetworks{ 0 };
    };
}

namespace winrt::midinetworksetup::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
