// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "ClockEngine.h"
#include "ClockItems.h"
#include "ClockStore.h"
#include "TapTempo.h"

namespace winrt::midiclock::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void RestoreWindowPlacement() noexcept;

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnSettingsToggleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnAddClockClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTileTapped(foundation::IInspectable const& sender, input::TappedRoutedEventArgs const& args);
        void OnTileEditClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTileStartStopClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnStartAllClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnStartSelectedClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnStopAllClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- the editor, in MainWindowEditor.cpp ----
        void OnEditDialogSaveClick(
            controls::ContentDialog const& sender,
            controls::ContentDialogButtonClickEventArgs const& args);

        void OnEditDialogDeleteClick(
            controls::ContentDialog const& sender,
            controls::ContentDialogButtonClickEventArgs const& args);

        void OnEditTempoValueChanged(
            controls::NumberBox const& sender,
            controls::NumberBoxValueChangedEventArgs const& args);

        void OnEditTempoSliderChanged(
            foundation::IInspectable const& sender,
            controls::Primitives::RangeBaseValueChangedEventArgs const& args);

        void OnEditEndpointSelectionChanged(
            foundation::IInspectable const& sender,
            controls::SelectionChangedEventArgs const& args);

        void OnTapTempoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        void InitializeWindowChrome() noexcept;
        void InitializeStaticText() noexcept;

        void StartEndpointWatcher() noexcept;
        void StopEndpointWatcher() noexcept;
        void RefreshEndpointList() noexcept;

        // Rebuilds the tiles from the saved clocks, keeping the selection and run state of any
        // clock that is still there.
        void RebuildTiles() noexcept;
        void RefreshTileText() noexcept;
        void UpdateEmptyState() noexcept;
        void UpdateStatus(winrt::hstring const& message) noexcept;
        void ReportStoreError() noexcept;

        winrt::midiclock::ClockItem ItemFromSender(foundation::IInspectable const& sender) const noexcept;
        winrt::midiclock::ClockItem ItemFromId(std::wstring const& id) const noexcept;

        // Present endpoints only. Empty when the device is not currently there.
        std::optional<midi2enum::MidiEndpointDeviceInformation> FindEndpointDevice(
            std::wstring const& endpointDeviceId) const noexcept;

        // Resolves the saved group choice against the endpoint, so "every declared group"
        // becomes a concrete list before anything is sent.
        std::vector<uint8_t> ResolveGroupIndexes(::midiclock::ClockDefinition const& definition) const noexcept;

        winrt::fire_and_forget StartClocksAsync(std::vector<std::wstring> ids);
        winrt::fire_and_forget StopClocksAsync(std::vector<std::wstring> ids);

        std::vector<std::wstring> AllClockIds() const noexcept;

        void SetBusy(std::vector<std::wstring> const& ids, bool isBusy) noexcept;
        void ApplyStartResults(std::map<std::wstring, ::midiclock::ClockStartResult> const& results) noexcept;

        // ---- the editor, in MainWindowEditor.cpp ----
        winrt::fire_and_forget ShowEditorAsync(std::wstring id);
        void PopulateEditor(::midiclock::ClockDefinition const& definition) noexcept;
        void RefreshEditorGroupList(int32_t desiredGroupIndex) noexcept;
        ::midiclock::ClockDefinition ReadEditor() noexcept;
        void SetEditorTempo(double beatsPerMinute) noexcept;

        void ApplyStartupOptions() noexcept;

        midiapp::WindowChrome m_chrome{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        ::midiclock::ClockEngine m_engine{};
        ::midiclock::TapTempo m_tapTempo{};

        collections::IObservableVector<winrt::midiclock::ClockItem> m_items{ nullptr };
        collections::IObservableVector<appshared::EndpointChoice> m_endpoints{ nullptr };
        collections::IObservableVector<appshared::NamedChoice> m_groups{ nullptr };

        // parallel to m_endpoints, so a picked row can be turned back into its device
        std::vector<midi2enum::MidiEndpointDeviceInformation> m_endpointDevices{};

        midi2enum::MidiEndpointDeviceWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        // empty while adding a clock that does not exist yet
        std::wstring m_editingId{};

        // A watcher refresh rebuilds the endpoint list, which would reset the picker the
        // customer is part way through using.
        bool m_editorOpen{ false };

        bool m_suppressTempoHandlers{ false };
        bool m_initialized{ false };
        bool m_startupOptionsApplied{ false };
    };
}

namespace winrt::midiclock::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
