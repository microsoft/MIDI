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
#include "KeyboardRollRenderer.h"
#include "midi_sequence_playback_engine.h"
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
        void OnKeyboardViewToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void ApplyViewMode() noexcept;

        void OnPositionSliderPointerPressed(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnPositionSliderPointerReleased(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args);
        void OnPositionSliderValueChanged(
            foundation::IInspectable const& sender,
            controls::Primitives::RangeBaseValueChangedEventArgs const& args);

        // Page Up and Page Down for both sliders, which WinUI's slider ignores.
        void OnSliderPreviewKeyDown(_In_ foundation::IInspectable const& sender, _In_ input::KeyRoutedEventArgs const& args);

        // Go to a time, from Ctrl+J.
        void OnGoToTimeDialogOpened(_In_ controls::ContentDialog const& sender, _In_ controls::ContentDialogOpenedEventArgs const& args);
        void OnGoToTimePrimaryClick(_In_ controls::ContentDialog const& sender, _In_ controls::ContentDialogButtonClickEventArgs const& args);

        void OnSpeedSliderValueChanged(
            _In_ foundation::IInspectable const& sender,
            _In_ controls::Primitives::RangeBaseValueChangedEventArgs const& args);
        void OnResetSpeedClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnTransposeSliderValueChanged(
            _In_ foundation::IInspectable const& sender,
            _In_ controls::Primitives::RangeBaseValueChangedEventArgs const& args);
        void OnResetTransposeClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        // Puts speed and transpose side by side when they fit, and one above the other when not.
        void OnNowPlayingPanelSizeChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::SizeChangedEventArgs const& args);

        // Keyboard shortcuts. Each is defined on the control it stands in for.
        void OnPlayPauseShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnStopShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnPreviousShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnNextShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnSeekBackShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnSeekForwardShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnSlowerShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnFasterShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnNormalSpeedShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnTransposeUpShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnTransposeDownShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnOriginalKeyShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnRepeatShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnGoToTimeShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);

        // About this song, in MainWindowSongInfo.cpp.
        void OnSongInfoShortcut(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnSongInfoFlyoutOpening(_In_ foundation::IInspectable const& sender, _In_ foundation::IInspectable const& args);
        void UpdateSongInfoButton() noexcept;
        std::shared_ptr<midifile::MidiSequence const> SongInfoSequence();
        std::shared_ptr<midifile::MidiSequence const> m_songInfoSequence{};
        std::wstring m_songInfoSequenceId{};

        void OnEndpointSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnGroupSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

        // GM Reset, in MainWindowGmReset.cpp.
        winrt::fire_and_forget OnGmResetClick(_In_ foundation::IInspectable sender, _In_ xaml::RoutedEventArgs args);
        void OnGroupComboBoxSizeChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::SizeChangedEventArgs const& args);

    private:
        // GM Reset, in MainWindowGmReset.cpp. The setting's toggle goes in the settings flyout,
        // and the reset is armed for the next start whenever a file is made ready to play.
        xaml::UIElement BuildGmResetSetting() noexcept;
        void QueueGmResetBeforeNextStart(_In_ uint8_t groupIndex) noexcept;
        bool m_gmResetSending{ false };

        void InitializeWindowChrome() noexcept;
        void InitializeStaticText() noexcept;
        void SingleInstancePublish() noexcept;

        // The playback part of the settings flyout, below the shared appearance controls.
        xaml::UIElement BuildPlaybackSettingsPanel() noexcept;

        // Hands the saved chase choice to the engine.
        void ApplyChaseSetting() noexcept;

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
        void UpdateTempoDisplay(double beatsPerMinute) noexcept;
        void UpdateSpeedDisplay() noexcept;
        void UpdateTransposeDisplay() noexcept;
        void UpdateLyricDisplay(uint32_t tick) noexcept;
        void UpdateNowPlayingText() noexcept;
        void UpdateTransportState() noexcept;
        void UpdatePositionDisplay() noexcept;
        void UpdateQueueVisibility() noexcept;

        // With nothing queued, in MainWindowEmptyState.cpp.
        void UnloadCurrentFile() noexcept;
        void UpdateEmptyLayout() noexcept;
        bool m_emptyLayoutShown{ false };

        // What the keyboard shortcuts share. Focus does not move when a shortcut is used, so each
        // one says what it did.
        void SeekFromShortcut(_In_ int64_t offsetMicroseconds) noexcept;
        void SetSpeedFromShortcut(_In_ double percent) noexcept;
        void SetTransposeFromShortcut(_In_ int32_t semitones) noexcept;
        void AnnounceShortcutResult(_In_ winrt::hstring const& message) noexcept;
        void AnnounceCurrentTitle() noexcept;

        // Says where playback is now, unless the position slider has focus: a screen reader
        // already reads the slider's new value then, and would otherwise say it twice.
        void AnnouncePosition() noexcept;
        bool PositionSliderHasFocus() noexcept;

        winrt::fire_and_forget ShowGoToTimeAsync();
        void ShowGoToTimeProblem(_In_ winrt::hstring const& message) noexcept;

        void StartPositionTimer() noexcept;
        void StopPositionTimer() noexcept;

        void ShowStatus(winrt::hstring const& message, controls::InfoBarSeverity severity) noexcept;
        void ClearStatus() noexcept;

        // Blocking. Background thread only.
        bool EnsureSession() noexcept;

        void ApplyStartupOptions() noexcept;

        HWND WindowHandle() noexcept;

        midiapp::WindowChrome m_chrome{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        ::midiplayer::PlaybackEngine m_engine{};
        ::midiplayer::PlayQueue m_queue{};

        // The engine borrows a session rather than making one of its own, so the app owns it and
        // keeps it for the life of the window. Creating it talks to the service, so it is only
        // ever touched from a background thread.
        winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };

        // Only the file being played is held as a full sequence; the queue keeps summaries.
        std::shared_ptr<midifile::MidiSequence const> m_currentSequence{};
        std::wstring m_currentSequenceId{};

        collections::IObservableVector<winrt::midiplayer::QueueItem> m_items{ nullptr };
        collections::IObservableVector<winrt::midiplayer::TrackItem> m_tracks{ nullptr };
        collections::IObservableVector<appshared::EndpointChoice> m_endpoints{ nullptr };
        collections::IObservableVector<appshared::NamedChoice> m_groups{ nullptr };

        ::midiplayer::NoteRollRenderer m_noteRoll{};
        ::midiplayer::KeyboardRollRenderer m_keyboardRoll{};

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
        bool m_keyboardView{ false };
        bool m_busy{ false };

        // The clock text only needs rewriting when the second changes, not on every frame. The
        // length is tracked too, so a new file shows its own length even when both start at 0:00.
        uint64_t m_lastDisplayedSecond{ UINT64_MAX };
        uint64_t m_lastDisplayedDuration{ UINT64_MAX };

        // Reused every frame so the activity update does not allocate.
        std::vector<uint8_t> m_soundingCounts{};
        midifile::TextEvent const* m_lastChord{ nullptr };
        int32_t m_lastDisplayedTempo{ -1 };
        midifile::LyricLine const* m_lastLyric{ nullptr };

        // The saved endpoint was not there when the app started. Playback waits for a choice
        // rather than picking something else and sending notes to the wrong instrument.
        bool m_savedEndpointMissing{ false };

        // Set when the play shortcut had to load the first file before it could start, so that
        // it can say "Playing" once it actually does.
        bool m_announceNextPlay{ false };

        // Go to time. Only one dialog can be open at once, and holding Ctrl+J down would ask for
        // more. The target is what the Go button accepted, in microseconds.
        bool m_goToTimeOpen{ false };
        std::optional<uint64_t> m_goToTimeTarget{};
    };
}

namespace winrt::midiplayer::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
