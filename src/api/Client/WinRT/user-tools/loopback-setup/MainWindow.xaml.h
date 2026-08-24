// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "LoopbackItems.h"
#include "ConfigFile.h"

namespace winrt::midiloopbacksetup::implementation
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

        winrt::fire_and_forget OnCreateLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCreateLoopbackFieldChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);

        winrt::fire_and_forget OnCreateDefaultLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnCreateDefaultBasicLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnCreateBasicLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCreateBasicLoopbackFieldChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);

        winrt::fire_and_forget OnChooseLoopbackImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnClearLoopbackImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnChooseBasicLoopbackImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnClearBasicLoopbackImageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnToggleMuteClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnToggleBasicMuteClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnDeleteLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDeleteBasicLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnLoopbacksDragStarting(
            foundation::IInspectable const& sender,
            controls::DragItemsStartingEventArgs const& args);

        void OnBasicLoopbacksDragStarting(
            foundation::IInspectable const& sender,
            controls::DragItemsStartingEventArgs const& args);

        void OnLoopbacksReordered(
            foundation::IInspectable const& sender,
            controls::DragItemsCompletedEventArgs const& args);

        void OnBasicLoopbacksReordered(
            foundation::IInspectable const& sender,
            controls::DragItemsCompletedEventArgs const& args);

        void OnMoveLoopbackUp(
            xaml::Input::KeyboardAccelerator const& sender,
            xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        void OnMoveLoopbackDown(
            xaml::Input::KeyboardAccelerator const& sender,
            xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        void OnMoveBasicLoopbackUp(
            xaml::Input::KeyboardAccelerator const& sender,
            xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        void OnMoveBasicLoopbackDown(
            xaml::Input::KeyboardAccelerator const& sender,
            xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

    private:
        // What one transport reports about itself and about the loopbacks it is running. Both
        // transports are gathered together in one background pass, because the customer can
        // switch pages faster than a poll interval.
        struct TransportSnapshot
        {
            bool Available{ false };
            bool CanMute{ false };
            bool CanList{ false };

            // the transport honors a picture given at creation, so the option is worth offering
            bool CanSetImage{ false };
            // the well known default loopback is already on this PC, so there is nothing to offer
            bool DefaultExists{ false };
            // association identifiers the configuration file has an entry for, lowercase and
            // unbraced so they compare directly with what the service reports
            std::vector<std::wstring> ConfiguredIds{};
            std::unordered_map<std::wstring, int32_t> DisplayOrders{};
        };

        struct ServiceSnapshot
        {
            bool Gathered{ false };

            TransportSnapshot Loopback{};
            TransportSnapshot BasicLoopback{};

            collections::IVectorView<midi2loop::MidiLoopbackEntry> LoopbackEntries{ nullptr };
            collections::IVector<midi2bloop::MidiBasicLoopbackEntry> BasicLoopbackEntries{ nullptr };
        };

        void StartRefreshTimer() noexcept;
        void StopRefreshTimer() noexcept;

        winrt::fire_and_forget RequestRefreshAsync() noexcept;
        static ServiceSnapshot GatherSnapshot() noexcept;

        void ApplySnapshot(ServiceSnapshot const& snapshot) noexcept;
        void ApplyLoopbacks(ServiceSnapshot const& snapshot) noexcept;
        void ApplyBasicLoopbacks(ServiceSnapshot const& snapshot) noexcept;

        void SelectUsablePageOnce(ServiceSnapshot const& snapshot) noexcept;

        // Folds the rows the service reported into an existing collection: matches by
        // association id, updates in place, adds what is new, drops what is gone, and puts the
        // result in the order the customer arranged.
        void ReconcileRows(
            _In_ collections::IObservableVector<midiloopbacksetup::LoopbackItem> const& rows,
            _In_ std::vector<::midiloopbacksetup::LoopbackRowData> const& incoming,
            _In_ std::unordered_map<std::wstring, int32_t> const& fileDisplayOrders,
            _In_ std::unordered_map<std::wstring, int32_t> const& sessionDisplayOrders,
            _In_ bool const canMute) noexcept;

        void ShowLoopbackPage(bool const showLoopbacks) noexcept;

        void SetLoopbackStatus(winrt::hstring const& text) noexcept;
        void SetBasicLoopbackStatus(winrt::hstring const& text) noexcept;

        // A status line describes something that just happened, so it stops being true within
        // seconds. Shows the text, then fades it away rather than leaving a stale claim on screen.
        void ShowTransientStatus(
            winrt::Microsoft::UI::Xaml::Controls::TextBlock const& target,
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer& timer,
            winrt::hstring const& text) noexcept;

        // yes / cancel confirmation, used before anything destructive
        foundation::IAsyncOperation<bool> ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message);

        void UpdateCreateLoopbackButtonState() noexcept;
        void UpdateCreateBasicLoopbackButtonState() noexcept;

        // Shows the chosen picture next to the picker, or clears it. The file has already been
        // copied into the shared folder by this point, so this works from the stored name.
        void ShowChosenImage(
            _In_ controls::Image const& preview,
            _In_ controls::TextBlock const& caption,
            _In_ controls::Button const& clearButton,
            _In_ winrt::hstring const& fileName) noexcept;

        winrt::fire_and_forget ChooseImageAsync(
            _In_ bool const forBasicLoopback);

        void PersistDisplayOrder(
            _In_ ::midiloopbacksetup::LoopbackKind const kind,
            _In_ collections::IObservableVector<midiloopbacksetup::LoopbackItem> const& rows) noexcept;

        // Moves whichever row has keyboard focus. Returns false when focus is not on a row, so
        // the accelerator can leave the key to whatever else wants it.
        bool MoveFocusedRow(
            _In_ controls::ListView const& list,
            _In_ collections::IObservableVector<midiloopbacksetup::LoopbackItem> const& rows,
            _In_ ::midiloopbacksetup::LoopbackKind const kind,
            _In_ int32_t const delta) noexcept;

        winrt::fire_and_forget SetMutedAsync(
            midiloopbacksetup::LoopbackItem const item,
            ::midiloopbacksetup::LoopbackKind const kind,
            bool const mute);

        winrt::fire_and_forget DeleteAsync(
            midiloopbacksetup::LoopbackItem const item,
            ::midiloopbacksetup::LoopbackKind const kind);

        midiapp::WindowChrome m_chrome{};

        controls::ContentDialog m_openDialog{ nullptr };

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_loopbackStatusTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_basicLoopbackStatusTimer{ nullptr };

        collections::IObservableVector<midiloopbacksetup::LoopbackItem> m_loopbacks{
            winrt::single_threaded_observable_vector<midiloopbacksetup::LoopbackItem>() };

        collections::IObservableVector<midiloopbacksetup::LoopbackItem> m_basicLoopbacks{
            winrt::single_threaded_observable_vector<midiloopbacksetup::LoopbackItem>() };

        // Positions the customer set during this session. They take precedence over what the
        // file says, because a loopback which was never saved has nowhere in the file to record
        // a position and would otherwise jump back on the next poll.
        std::unordered_map<std::wstring, int32_t> m_loopbackOrder{};
        std::unordered_map<std::wstring, int32_t> m_basicLoopbackOrder{};

        std::atomic<bool> m_refreshInProgress{ false };

        bool m_loaded{ false };
        bool m_closing{ false };

        // the opening page may be moved off an unavailable transport, but only once
        bool m_appliedPageFallback{ false };

        // picture chosen in the create dialog, already copied into the shared assets folder
        winrt::hstring m_pendingLoopbackImage{};
        winrt::hstring m_pendingBasicLoopbackImage{};

        // a drag reorders the collection the list is bound to, so a refresh landing mid drag
        // would fight the customer for it
        bool m_reordering{ false };
    };
}

namespace winrt::midiloopbacksetup::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
