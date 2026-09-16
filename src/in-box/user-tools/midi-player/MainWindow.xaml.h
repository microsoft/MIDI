// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "NoteRollRenderer.h"
#include "PlaybackEngine.h"
#include "PlayQueue.h"
#include "QueueItem.h"
#include "TrackItem.h"

namespace winrt::midiplayer::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void RestoreWindowPlacement() noexcept;

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnSettingsToggleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnRootDragOver(foundation::IInspectable const& sender, xaml::DragEventArgs const& args);
        winrt::fire_and_forget OnRootDrop(foundation::IInspectable sender, xaml::DragEventArgs args);

        winrt::fire_and_forget OnOpenFilesClick(foundation::IInspectable sender, xaml::RoutedEventArgs args);
        void OnClearQueueClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRemoveQueueItemClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnQueueItemDoubleTapped(foundation::IInspectable const& sender, input::DoubleTappedRoutedEventArgs const& args);

        void OnTrackMuteClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTrackSoloClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnNoteRollSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);

        void OnPlayPauseClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnStopClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnPreviousClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnNextClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRepeatToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnQueueToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnPositionSliderPointerPressed(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnPositionSliderPointerReleased(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnPositionSliderValueChanged(
            foundation::IInspectable const& sender,
            controls::Primitives::RangeBaseValueChangedEventArgs const& args);

        void OnEndpointSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnGroupSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

    private:
        void InitializeWindowChrome() noexcept;
        void InitializeStaticText() noexcept;
        void SingleInstancePublish() noexcept;

        // Files handed over by a second launch arrive as WM_COPYDATA, which XAML does not
        // surface, so the window's Win32 handle is subclassed for it.
        static LRESULT CALLBACK SubclassProcedure(
            HWND window,
            UINT message,
            WPARAM wParam,
            LPARAM lParam,
            UINT_PTR subclassId,
            DWORD_PTR referenceData) noexcept;

        void HandleFilesFromOtherInstance(_In_ std::vector<std::wstring> const& paths) noexcept;

        static winrt::weak_ref<winrt::midiplayer::MainWindow> s_instance;

        void StartEndpointWatcher() noexcept;
        void StopEndpointWatcher() noexcept;
        void RefreshEndpointList() noexcept;
        void RefreshGroupList(int32_t desiredGroupIndex) noexcept;

        std::optional<midi2enum::MidiEndpointDeviceInformation> SelectedEndpoint() noexcept;
        std::wstring SelectedEndpointDeviceId() noexcept;

        // Queue and playback, all on the UI thread unless noted.
        winrt::fire_and_forget AddFilesAsync(std::vector<std::wstring> paths, bool playWhenReady);
        winrt::fire_and_forget StartCurrentAsync(bool autoPlay);

        void HandlePlaybackCompleted() noexcept;

        void RebuildQueueList() noexcept;
        void UpdateCurrentQueueRow() noexcept;

        void RebuildTrackList() noexcept;
        void ApplyTrackStatesToEngine() noexcept;
        void RefreshTrackRowStates() noexcept;
        void RenderNoteRoll() noexcept;

        // Both driven from the position timer, so both are on the frame path.
        void UpdateTrackActivity(uint32_t tick) noexcept;
        void UpdateChordDisplay(uint32_t tick) noexcept;
        void UpdateNowPlayingText() noexcept;
        void UpdateTransportState() noexcept;
        void UpdatePositionDisplay() noexcept;
        void UpdateQueueVisibility() noexcept;

        void StartPositionTimer() noexcept;
        void StopPositionTimer() noexcept;

        void ShowStatus(winrt::hstring const& message, controls::InfoBarSeverity severity) noexcept;
        void ClearStatus() noexcept;

        void ApplyStartupOptions() noexcept;

        HWND WindowHandle() noexcept;

        midiapp::WindowChrome m_chrome{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        ::midiplayer::PlaybackEngine m_engine{};
        ::midiplayer::PlayQueue m_queue{};

        // Only the file being played is held as a full sequence; the queue keeps summaries.
        std::shared_ptr<midifile::MidiSequence const> m_currentSequence{};
        std::wstring m_currentSequenceId{};

        collections::IObservableVector<winrt::midiplayer::QueueItem> m_items{ nullptr };
        collections::IObservableVector<winrt::midiplayer::TrackItem> m_tracks{ nullptr };
        collections::IObservableVector<appshared::EndpointChoice> m_endpoints{ nullptr };
        collections::IObservableVector<appshared::NamedChoice> m_groups{ nullptr };

        ::midiplayer::NoteRollRenderer m_noteRoll{};

        // parallel to m_endpoints, so a picked row can be turned back into its device
        std::vector<midi2enum::MidiEndpointDeviceInformation> m_endpointDevices{};

        midi2enum::MidiEndpointDeviceWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};
        winrt::event_token m_watcherEnumerationCompletedToken{};

        // The watcher raises Added once per device, so a list part way through enumeration is
        // not evidence that anything is missing.
        bool m_enumerationCompleted{ false };

        // A file named on the command line, opened before any endpoint had turned up yet.
        bool m_pendingAutoPlay{ false };

        xaml::DispatcherTimer m_positionTimer{ nullptr };

        bool m_initialized{ false };
        bool m_startupOptionsApplied{ false };

        // A watcher refresh rebuilds the endpoint list, which would otherwise reset the picker
        // and look like the customer's choice was thrown away.
        bool m_suppressEndpointHandlers{ false };
        bool m_suppressGroupHandlers{ false };
        bool m_suppressPositionHandlers{ false };

        bool m_scrubbing{ false };
        bool m_busy{ false };

        // The clock text only needs rewriting when the second changes, not on every frame.
        uint64_t m_lastDisplayedSecond{ UINT64_MAX };

        // Reused every frame so the activity update does not allocate.
        std::vector<uint8_t> m_soundingCounts{};
        midifile::TextEvent const* m_lastChord{ nullptr };

        // The saved endpoint was not there when the app started. Playback waits for a choice
        // rather than picking something else and sending notes to the wrong instrument.
        bool m_savedEndpointMissing{ false };
    };
}

namespace winrt::midiplayer::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
