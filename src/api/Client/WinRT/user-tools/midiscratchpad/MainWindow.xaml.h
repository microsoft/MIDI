// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "ScratchPadParser.h"

namespace winrt::midiscratchpad::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);

        void OnEndpointSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnGroupSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnEndpointChoiceLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnModeChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnEditorTextChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnEditorSelectionChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRunningStatusToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnAddNoteOn(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddNoteOff(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddControlChange(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddProgramChange(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddPitchBend(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddChannelPressure(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddAllNotesOff(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddIdentityRequest(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddEmptySysEx(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnClearClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSendAllClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSendSelectionClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // called before Activate so the window never appears at the wrong size first
        void RestoreWindowPlacement() noexcept;

    private:
        void InitializeWindowChrome() noexcept;
        void InitializeCollections() noexcept;
        void InitializeControlsFromSettings() noexcept;
        void ApplyStartupOptions() noexcept;

        void StartEndpointWatcher() noexcept;
        void StopEndpointWatcher() noexcept;
        void RefreshEndpointList() noexcept;
        void RefreshGroupList() noexcept;

        winrt::hstring SelectedEndpointDeviceId() noexcept;
        uint8_t SelectedGroupNumber() noexcept;
        uint8_t SelectedChannelNumber() noexcept;

        ::midiscratchpad::ScratchPadMode CurrentMode() noexcept;
        void ApplyModeToLayout() noexcept;

        void UpdateParseStatus() noexcept;
        void UpdateCommandStates() noexcept;
        void UpdateWindowTitle() noexcept;

        void AppendText(std::wstring const& text) noexcept;
        void SendText(winrt::hstring const& text, bool isSelection) noexcept;
        void ShowSendResult(winrt::hstring const& message, bool isError) noexcept;

        void ShowMessageAsync(winrt::hstring title, winrt::hstring message);

        midiapp::WindowChrome m_chrome{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        midi2::MidiSession m_session{ nullptr };

        midi2enum::MidiEndpointDeviceWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        collections::IObservableVector<appshared::EndpointChoice> m_endpoints{ nullptr };
        collections::IObservableVector<appshared::NamedChoice> m_groups{ nullptr };
        std::vector<midi2enum::MidiEndpointDeviceInformation> m_endpointDevices{};

        ::midiscratchpad::ParsedInput m_lastParse{};

        bool m_initialized{ false };
        bool m_suppressSelectionHandling{ false };
        bool m_suppressModeHandling{ false };
        bool m_startupOptionsApplied{ false };
        winrt::hstring m_selectionText{};
    };
}

namespace winrt::midiscratchpad::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
