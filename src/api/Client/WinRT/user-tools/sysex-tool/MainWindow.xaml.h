// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "SysExBuffer.h"

namespace winrt::midisysextool::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);

        void OnEndpointSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnGroupSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnEndpointChoiceLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnTaskModeChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTransferSettingChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnBrowseSendFileClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnPrimaryClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSaveClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnClearClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnDumpContainerContentChanging(controls::ListViewBase const& sender, controls::ContainerContentChangingEventArgs const& args);

        // called before Activate so the window never appears at the wrong size first
        void RestoreWindowPlacement() noexcept;

    private:
        void InitializeWindowChrome() noexcept;
        void InitializeCollections() noexcept;
        void InitializeControlsFromSettings() noexcept;

        void StartEndpointWatcher() noexcept;
        void StopEndpointWatcher() noexcept;
        void RefreshEndpointList() noexcept;
        void RefreshGroupList() noexcept;

        winrt::hstring SelectedEndpointDeviceId() noexcept;
        uint8_t SelectedGroupNumber() noexcept;

        bool IsReceiveTask() noexcept;
        void ApplyTaskToLayout() noexcept;

        void UpdateCommandStates() noexcept;
        void UpdateWindowTitle() noexcept;
        void UpdateStatus() noexcept;

        // opens a connection on the selected endpoint, reusing the one already open when it
        // still points at the same device
        bool EnsureConnection() noexcept;
        void CloseConnection() noexcept;

        winrt::fire_and_forget SendFileAsync();
        void CancelSend() noexcept;
        void ShowSendProgress(uint64_t bytesRead, uint64_t messagesSent) noexcept;

        void StartReceiving() noexcept;
        void StopReceiving() noexcept;
        void OnBytesReceived(std::vector<uint8_t> const& bytes) noexcept;
        void OnSysEx7WordsReceived(uint32_t word0, uint32_t word1) noexcept;

        void ResetDisplay() noexcept;
        void RefreshDisplay() noexcept;
        void AppendRows() noexcept;
        void ScheduleDisplayRefresh() noexcept;
        void UpdateRowBrushes() noexcept;

        winrt::fire_and_forget SaveReceivedAsync();
        void SetLibraryFolder(std::wstring const& folder) noexcept;

        void ShowMessageAsync(winrt::hstring title, winrt::hstring message);

        midiapp::WindowChrome m_chrome{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_displayTimer{ nullptr };

        midi2::MidiSession m_session{ nullptr };
        midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::hstring m_connectedEndpointId{};

        midi2sysex::MidiSystemExclusiveReceiver m_receiver{ nullptr };
        winrt::event_token m_receiverToken{};
        winrt::event_token m_messageReceivedToken{};

        foundation::IAsyncOperationWithProgress<bool, midi2sysex::MidiSystemExclusiveSendProgress> m_sendOperation{ nullptr };

        midi2enum::MidiEndpointDeviceWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        collections::IObservableVector<appshared::EndpointChoice> m_endpoints{ nullptr };
        collections::IObservableVector<appshared::NamedChoice> m_groups{ nullptr };
        std::vector<midi2enum::MidiEndpointDeviceInformation> m_endpointDevices{};

        collections::IObservableVector<midisysextool::SysExRow> m_rows{ nullptr };

        // F0 and F7, picked from the live theme rather than a resource lookup
        media::SolidColorBrush m_framingBrush{ nullptr };
        media::SolidColorBrush m_dataBrush{ nullptr };

        ::midisysextool::SysExBuffer m_buffer{};

        // how much of the buffer has already been turned into rows
        size_t m_renderedByteCount{ 0 };
        size_t m_renderedWordCount{ 0 };
        bool m_displayTruncated{ false };

        // tracks whether a SysEx is open, so out of sequence packets can be flagged
        bool m_rowInsideMessage{ false };

        uint64_t m_sendTotalBytes{ 0 };
        bool m_isSending{ false };

        bool m_initialized{ false };
        bool m_suppressSelectionHandling{ false };
        bool m_suppressModeHandling{ false };
        bool m_startupOptionsApplied{ false };

        std::wstring m_sendFilePath{};

        // a dump can outrun the display, so rows are added on a timer rather than per event
        static constexpr int64_t DisplayRefreshMilliseconds = 100;

        // bounds the row list no matter how large the dump; the buffer still keeps every byte
        static constexpr size_t MaximumDisplayRows = 20000;
    };
}

namespace winrt::midisysextool::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
