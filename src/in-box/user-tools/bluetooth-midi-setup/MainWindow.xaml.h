// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include <limits>

#include "AppSettings.h"
#include "BluetoothItems.h"
#include "WindowChrome.h"

namespace winrt::midibluetoothsetup::implementation
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

        winrt::fire_and_forget OnConnectDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDisconnectDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnForgetDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnCustomizeDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBrowseForImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRemoveImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnOfflineRetentionChanged(foundation::IInspectable const& sender, xaml::Controls::SelectionChangedEventArgs const& args);
        winrt::fire_and_forget OnDefaultOfflineRetentionChanged(foundation::IInspectable const& sender, xaml::Controls::SelectionChangedEventArgs const& args);
        void OnCopyEndpointDeviceIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorEndpointClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnStartPeripheralClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnStopPeripheralClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnCustomizePeripheralClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyPeripheralEndpointDeviceIdClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnAllowClientOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnAllowClientAlwaysClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDenyClientOnceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBlockClientClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        // everything the service knows, gathered off the UI thread in one pass
        struct ServiceSnapshot
        {
            bool TransportAvailable{ false };
            bool Gathered{ false };

            collections::IVectorView<midi2bt::MidiBluetoothDeviceInformation> Devices{ nullptr };
            midi2bt::MidiBluetoothPeripheralStatus Peripheral{ nullptr };

            // remote devices which have connected and are waiting for a decision
            collections::IVectorView<midi2bt::MidiBluetoothPeripheralClient> PendingClients{ nullptr };

            // null when the transport did not report it, which is what an older service looks
            // like. Reporting that as "no radio" would be worse than saying nothing.
            midi2bt::MidiBluetoothRadioInformation Radio{ nullptr };

            // addresses the configuration file remembers, lowercased
            std::vector<std::wstring> RememberedDeviceIds{};
        };

        void StartRefreshTimer() noexcept;
        void StopRefreshTimer() noexcept;

        winrt::fire_and_forget RequestRefreshAsync() noexcept;
        static ServiceSnapshot GatherSnapshot() noexcept;

        void ApplySnapshot(ServiceSnapshot const& snapshot) noexcept;
        void ApplyDevices(ServiceSnapshot const& snapshot) noexcept;
        void ApplyPeripheral(ServiceSnapshot const& snapshot) noexcept;
        void ApplyPendingClients(ServiceSnapshot const& snapshot) noexcept;
        void ApplyRadio(ServiceSnapshot const& snapshot) noexcept;

        winrt::Windows::Foundation::IAsyncOperation<bool> DecideClientAsync(
            _In_ foundation::IInspectable const& sender,
            _In_ bool const approve,
            _In_ midi2bt::MidiBluetoothApprovalScope const scope) noexcept;

        void ShowPage(uint32_t const pageIndex) noexcept;

        void SetDevicesStatus(winrt::hstring const& text) noexcept;
        void SetPeripheralStatus(winrt::hstring const& text) noexcept;

        // A status line describes something that just happened, so it stops being true within
        // seconds. Shows the text, then fades it away rather than leaving a stale claim on screen.
        void ShowTransientStatus(
            winrt::Microsoft::UI::Xaml::Controls::TextBlock const& target,
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer& timer,
            winrt::hstring const& text) noexcept;

        // False when the transport is missing. It has already shown the customer why by the
        // time it returns.
        bool VerifyTransportIsUsable() noexcept;

        // yes / cancel confirmation, used before anything destructive
        foundation::IAsyncOperation<bool> ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message);

        // shared by the device rows and the peripheral's connected client. By value, because a
        // coroutine does not keep reference parameters in its frame and these outlive a suspend.
        foundation::IAsyncOperation<bool> ShowCustomizeDialogAsync(
            winrt::hstring endpointDeviceInstanceId,
            winrt::hstring transportSuppliedName,
            winrt::hstring currentName,
            winrt::hstring currentDescription,
            winrt::hstring currentImage);

        midiapp::WindowChrome m_chrome{};

        // only one dialog can be open at a time, and a second ShowAsync throws
        controls::ContentDialog m_openDialog{ nullptr };

        collections::IObservableVector<midibluetoothsetup::BluetoothDeviceItem> m_devices{
            winrt::single_threaded_observable_vector<midibluetoothsetup::BluetoothDeviceItem>() };

        // one row, because the peripheral accepts a single connected client
        collections::IObservableVector<midibluetoothsetup::PeripheralClientItem> m_peripheralClients{
            winrt::single_threaded_observable_vector<midibluetoothsetup::PeripheralClientItem>() };

        collections::IObservableVector<midibluetoothsetup::PendingClientItem> m_pendingClients{
            winrt::single_threaded_observable_vector<midibluetoothsetup::PendingClientItem>() };

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_devicesStatusTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_peripheralStatusTimer{ nullptr };

        bool m_loaded{ false };
        bool m_closing{ false };

        // a tick is skipped rather than queued when the previous refresh is still running
        std::atomic<bool> m_refreshInFlight{ false };

        void ApplyTransportSettings() noexcept;

        bool m_transportUsable{ false };

        // What the service currently holds, so the combo's SelectionChanged can tell a real change
        // from the one it raises while being populated. The initial value must not be one any
        // setting can take, or the first refresh mistakes itself for a no-op and never populates.
        static constexpr int32_t RetentionNotLoaded{ std::numeric_limits<int32_t>::min() };

        int32_t m_defaultOfflineRetentionSeconds{ RetentionNotLoaded };
    };
}

namespace winrt::midibluetoothsetup::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
