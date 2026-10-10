// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "ArrangeLayout.h"
#include "ArrangeRenderer.h"
#include "EndpointDirectory.h"
#include "PianoRoll.h"
#include "PlaybackEngine.h"
#include "RecordingTake.h"
#include "SequenceEdits.h"
#include "SequenceModel.h"
#include "SequenceUndo.h"
#include "SessionEngineOutput.h"
#include "SourceMonitor.h"
#include "ThemePalette.h"

namespace winrt::midisequencer::implementation
{
    namespace seq = ::midisequencer;

    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void RestoreWindowPlacement() noexcept;

        // ---- XAML events ----
        void OnRootLoaded(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRootDragOver(_In_ foundation::IInspectable const& sender, _In_ xaml::DragEventArgs const& args);
        winrt::fire_and_forget OnRootDrop(_In_ foundation::IInspectable sender, _In_ xaml::DragEventArgs args);

        void OnSettingsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnNewClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnOpenSampleClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnOpenClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSaveClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSaveAsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportStandardMidiFileClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportClipFileClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnExportClipFilesClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnExportStandardMidiFileClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCloseWindowClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnGoToStartClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPlayClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnStopClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRecordClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnLoopClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnMetronomeClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnMetronomeMenuClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnClockClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnLaunchQuantizeChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnSnapChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnCaptureClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnBackToTimelineClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSilenceClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnUndoClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRedoClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnAddTrackClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnAddFolderClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnLauncherToggleClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnVerticalScrollChanged(_In_ foundation::IInspectable const& sender, _In_ primitives::RangeBaseValueChangedEventArgs const& args);
        void OnHorizontalScrollChanged(_In_ foundation::IInspectable const& sender, _In_ primitives::RangeBaseValueChangedEventArgs const& args);

        void OnInspectorCloseClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTrackNameCommitted(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTrackNameKeyDown(_In_ foundation::IInspectable const& sender, _In_ input::KeyRoutedEventArgs const& args);
        void OnTrackPinClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSourceChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnEchoToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRecordFromKeyboardClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnDestinationChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnTrySoundClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnEditorDrawClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnEditorQuantizeClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTransposeClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnEditorKeyboardClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnValuesAsChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnEditorCloseClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSplitterPressed(_In_ foundation::IInspectable const& sender, _In_ input::PointerRoutedEventArgs const& args);
        void OnSplitterMoved(_In_ foundation::IInspectable const& sender, _In_ input::PointerRoutedEventArgs const& args);
        void OnSplitterReleased(_In_ foundation::IInspectable const& sender, _In_ input::PointerRoutedEventArgs const& args);
        void OnSplitterCaptureLost(_In_ foundation::IInspectable const& sender, _In_ input::PointerRoutedEventArgs const& args);

    private:
        // ---- window (MainWindow.xaml.cpp) ----
        void InitializeStaticText() noexcept;
        void InitializeWindowChrome() noexcept;
        void InitializeKeyboard() noexcept;
        void ApplyPalette() noexcept;
        void UpdateTitle() noexcept;
        void UpdateStatusBar() noexcept;
        void ShowMessage(_In_ winrt::hstring const& text) noexcept;
        void OnFrame() noexcept;
        void OnServiceTimer() noexcept;
        void OnWindowClosing(_In_ winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args);
        void Shutdown() noexcept;
        HWND WindowHandle() noexcept;
        void OnKeyboardAccelerator(_In_ input::KeyboardAccelerator const& sender, _In_ input::KeyboardAcceleratorInvokedEventArgs const& args);

        // ---- the document (MainWindowDocument.cpp) ----
        void NewDocument() noexcept;
        void OpenSample() noexcept;
        void ShowNewDocument(_In_ seq::Sequence doc) noexcept;
        void OpenFirstClipInEditor() noexcept;
        winrt::fire_and_forget OpenFileAsync(std::wstring path);
        bool OpenSequenceText(_In_ std::wstring const& path, _In_ std::wstring const& text) noexcept;
        void ImportStandardMidiFile(_In_ std::wstring const& path) noexcept;
        void ImportClipFile(_In_ std::wstring const& path) noexcept;
        winrt::fire_and_forget SaveAsync(bool chooseName);
        winrt::fire_and_forget AutosaveAsync();

        // One sequence per window: a window can take another sequence only while it holds an
        // untouched new one. Otherwise the other sequence opens in a window of its own.
        bool CanReplaceDocument() const noexcept;
        void OpenInNewWindow(_In_ std::wstring const& path, _In_ bool sample = false) noexcept;
        std::wstring ShowFileDialog(_In_ bool save, _In_ std::wstring const& defaultName, _In_ std::vector<std::pair<std::wstring, std::wstring>> const& filters, _In_ bool folders = false) noexcept;
        std::wstring DocumentsFolder() noexcept;

        // Records the changes for undo, marks the sequence changed, and redraws.
        void Commit(_In_ std::wstring const& name, _In_ seq::ChangeList changes);
        void EditTracks(_In_ std::wstring const& name, _In_ std::function<void(seq::Sequence&)> const& edit);
        void DocumentChanged() noexcept;
        void Publish() noexcept;
        void Undo() noexcept;
        void Redo() noexcept;
        void UpdateUndoButtons() noexcept;
        void UpdateSavedState() noexcept;

        // ---- the arrangement (MainWindowArrange.cpp) ----
        void CreateCanvases();
        void RebuildLayout() noexcept;
        void RebuildHeaders() noexcept;
        void RebuildSceneHeaders() noexcept;
        void PositionHeaders() noexcept;
        void UpdateScrollBars() noexcept;
        void InvalidateArrange() noexcept;
        void InvalidateLaunchers() noexcept;
        seq::ArrangeDrawContext DrawContext();
        double LaneWidth() noexcept;
        double ScrollViewportHeight() noexcept;
        void SetZoom(_In_ double barWidth, _In_ double anchorX) noexcept;
        void ScrollToTick(_In_ int64_t tick) noexcept;
        void UpdatePlayhead() noexcept;
        xaml::UIElement MakeHeaderRow(_In_ seq::ArrangeRow const& row);
        void SelectTrack(_In_ std::wstring const& trackId) noexcept;
        void ToggleFolder(_In_ std::wstring const& trackId) noexcept;
        void ToggleTrackFlag(_In_ std::wstring const& trackId, _In_ wchar_t flag, _In_ bool value) noexcept;
        void ShowTrackMenu(_In_ std::wstring const& trackId, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        void AddTrack(_In_ bool folder) noexcept;
        void DeleteTrack(_In_ std::wstring const& trackId) noexcept;
        void BeginRenameTrack(_In_ std::wstring const& trackId) noexcept;

        void OnLanePressed(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnLaneMoved(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnLaneReleased(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnLaneWheel(_In_ input::PointerRoutedEventArgs const& args);
        void OnLaneDoubleTapped(_In_ bool pinned, _In_ input::DoubleTappedRoutedEventArgs const& args);
        void OnLaneRightTapped(_In_ bool pinned, _In_ input::RightTappedRoutedEventArgs const& args);
        void OnRulerPressed(_In_ input::PointerRoutedEventArgs const& args);
        void OnRulerMoved(_In_ input::PointerRoutedEventArgs const& args);
        void OnRulerReleased(_In_ input::PointerRoutedEventArgs const& args);
        void OnLauncherPressed(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnLauncherDoubleTapped(_In_ bool pinned, _In_ input::DoubleTappedRoutedEventArgs const& args);
        void OnLauncherRightTapped(_In_ bool pinned, _In_ input::RightTappedRoutedEventArgs const& args);

        struct LaneHit
        {
            seq::ArrangeRow const* Row{ nullptr };
            size_t Placement{ SIZE_MAX };
            bool OnRightEdge{ false };
            bool OnBackButton{ false };
            int64_t Tick{ 0 };
        };

        LaneHit HitTestLane(_In_ bool pinned, _In_ foundation::Point point) noexcept;
        void ShowPlacementMenu(_In_ std::wstring const& trackId, _In_ size_t placement, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        void ShowLaneMenu(_In_ seq::ArrangeRow const& row, _In_ int64_t tick, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        void CreateClipAt(_In_ std::wstring const& trackId, _In_ int64_t tick) noexcept;
        void DeleteSelectedPlacement() noexcept;
        void DuplicateSelectedPlacement() noexcept;
        void MakePlacementUnique(_In_ std::wstring const& trackId, _In_ size_t placement) noexcept;
        void AddTagAt(_In_ std::wstring const& trackId, _In_ int64_t tick) noexcept;
        void RenameClip(_In_ std::wstring const& clipId) noexcept;
        void AddScene() noexcept;
        void DeleteScene(_In_ size_t scene) noexcept;
        void LaunchScene(_In_ size_t scene) noexcept;
        void LaunchSlot(_In_ std::wstring const& trackId, _In_ size_t scene) noexcept;
        void CreateClipInSlot(_In_ std::wstring const& trackId, _In_ size_t scene) noexcept;
        void PlaceSlotClipOnTimeline(_In_ std::wstring const& trackId, _In_ size_t scene) noexcept;
        void DeleteSlotClip(_In_ std::wstring const& trackId, _In_ size_t scene) noexcept;
        void ShowSlotMenu(_In_ std::wstring const& trackId, _In_ size_t scene, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        std::optional<std::pair<std::wstring, size_t>> HitTestSlot(_In_ bool pinned, _In_ foundation::Point point) noexcept;
        void ShowTextPrompt(_In_ winrt::hstring const& title, _In_ winrt::hstring const& initial, _In_ std::function<void(std::wstring const&)> done);
        void OnArrangeSizeChanged() noexcept;
        void SetLauncherVisible(_In_ bool visible) noexcept;
        void UpdateLeds() noexcept;
        int64_t SnapGrid() const noexcept;
        std::wstring NextClipName() const;
        media::SolidColorBrush BrushFor(_In_ seq::Color color) const;
        media::SolidColorBrush HeaderBackground(_In_ seq::ArrangeRow const& row) const;
        media::Brush ThemeBrush(_In_ wchar_t const* key);

        // ---- the clip editor (MainWindowEditor.cpp) ----
        void OpenClipInEditor(_In_ std::wstring const& clipId, _In_ std::wstring const& trackId) noexcept;
        void CloseEditor() noexcept;
        void RefreshEditor() noexcept;
        void UpdateEditorHeader() noexcept;
        void UpdateNoteInspector() noexcept;
        void ApplyRollEdit(_In_ std::optional<seq::RollEdit> edit) noexcept;
        void OnEditorPressed(_In_ input::PointerRoutedEventArgs const& args);
        void OnEditorMoved(_In_ input::PointerRoutedEventArgs const& args);
        void OnEditorReleased(_In_ input::PointerRoutedEventArgs const& args);
        void OnEditorWheel(_In_ input::PointerRoutedEventArgs const& args);
        void OnEditorKeyDown(_In_ input::KeyRoutedEventArgs const& args);
        void InvalidateEditor() noexcept;
        seq::Track const* EditorTrack() const noexcept;

        // ---- playback, devices and recording (MainWindowPlayback.cpp) ----
        winrt::fire_and_forget StartMidiAsync();
        void RefreshEndpoints() noexcept;
        winrt::fire_and_forget PrepareConnectionsAsync();
        void ApplyEngineSettings() noexcept;
        void Play(_In_ int64_t fromTick) noexcept;
        void Stop() noexcept;
        void Silence() noexcept;
        void SetPosition(_In_ int64_t tick) noexcept;
        void UpdateTransport() noexcept;
        void UpdateDisplays(_In_ int64_t tick) noexcept;
        void StartRecording() noexcept;
        void StopRecording() noexcept;
        void OnSourceMessage(_In_ std::wstring const& endpointId, _In_ uint64_t timestamp, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;
        void UpdateEchoRoutes() noexcept;
        void SendNow(_In_ seq::TrackDestination const& destination, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;
        void AuditionNote(_In_ uint8_t note, _In_ bool on) noexcept;
        void LaunchKeyboard(_In_ std::wstring const& endpointId, _In_ uint8_t group, _In_ int8_t channel) noexcept;
        void ShowMetronomeFlyout();
        seq::LaunchQuantize LaunchQuantize() const noexcept;
        std::wstring DescribeDestination(_In_ seq::Track const& track) const;
        std::wstring DescribeSource(_In_ seq::Track const& track) const;

        // ---- the track inspector (MainWindowTrack.cpp) ----
        void ShowInspector(_In_ std::wstring const& trackId) noexcept;
        void HideInspector() noexcept;
        void RefreshInspector() noexcept;
        void FillEndpointCombo(_In_ controls::ComboBox const& combo, _In_ bool sources, _In_ seq::EndpointRef const& selected);
        void FillGroupCombo(_In_ controls::ComboBox const& combo, _In_ bool sources, _In_ seq::EndpointRef const& endpoint, _In_ int32_t selected, _In_ bool allowAny);
        void FillChannelCombo(_In_ controls::ComboBox const& combo, _In_ int32_t selected, _In_ bool allowAny);
        void UpdateProtocolNote() noexcept;

        // ---- state ----
        midiapp::WindowChrome m_chrome{};
        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_closeConfirmed{ false };

        seq::Sequence m_doc{};
        std::wstring m_path{};
        bool m_dirty{ false };
        bool m_readOnly{ false };
        std::optional<std::chrono::system_clock::time_point> m_savedAt{};
        bool m_autosaved{ false };
        seq::UndoStack m_undo{};
        uint64_t m_version{ 0 };

        // A new sequence's first track plays to the General MIDI Synth once it's been found.
        bool m_wantsDefaultDestination{ false };

        seq::ArrangeLayout m_layout{};
        double m_scrollY{ 0 };
        double m_scrollX{ 0 };
        double m_sceneScrollX{ 0 };
        double m_barWidth{ 34 };
        bool m_updatingScrollBars{ false };

        // The timeline loop, set by dragging on the ruler. Not part of the sequence.
        int64_t m_loopStart{ 0 };
        int64_t m_loopEnd{ 4 * 3840 };
        double m_rulerPressX{ 0 };
        bool m_rulerMoved{ false };

        // A take as it grows, for drawing.
        std::vector<seq::Note> m_recordingPreview{};
        std::wstring m_recordingPreviewTrackId{};
        int64_t m_recordingPreviewStart{ 0 };

        // Drawn rather than shown in XAML, so loaded once.
        winrt::hstring m_textPlayingLaunchedClip{};
        winrt::hstring m_textBackToTimeline{};
        winrt::hstring m_textBarFormat{};
        winrt::hstring m_textRecording{};
        winrt::hstring m_textGenerated{};

        std::unordered_map<std::wstring, shapes::Ellipse> m_leds{};
        controls::StackPanel m_sceneHeaderPanel{ nullptr };

        std::wstring m_selectedTrackId{};
        size_t m_selectedPlacement{ SIZE_MAX };
        std::wstring m_selectedSlotTrackId{};
        size_t m_selectedSlot{ SIZE_MAX };
        std::wstring m_editorClipId{};
        std::wstring m_editorTrackId{};
        std::wstring m_inspectorTrackId{};
        bool m_inspectorUpdating{ false };

        // A drag on the timeline.
        enum class LaneDrag : uint8_t { None, Move, Resize, Loop };
        LaneDrag m_laneDrag{ LaneDrag::None };
        bool m_lanePinned{ false };
        foundation::Point m_dragStart{};
        int64_t m_dragOriginalTick{ 0 };
        int64_t m_dragOriginalLength{ 0 };
        int64_t m_dragTick{ 0 };
        int64_t m_dragLength{ 0 };
        std::vector<seq::Track> m_dragTracksBefore{};
        int64_t m_loopDragStart{ 0 };
        bool m_rulerDragging{ false };

        bool m_splitterDragging{ false };
        double m_splitterStartY{ 0 };
        double m_splitterStartHeight{ 0 };

        seq::Palette m_palette{};
        seq::ArrangeRenderer m_renderer{};
        seq::PianoRoll m_roll{};
        std::optional<uint8_t> m_auditioning{};
        std::chrono::steady_clock::time_point m_lastEditorPressTime{};
        foundation::Point m_lastEditorPressPoint{};
        bool m_editorGesture{ false };

        canvasXaml::CanvasControl m_rulerCanvas{ nullptr };
        canvasXaml::CanvasControl m_pinnedLaneCanvas{ nullptr };
        canvasXaml::CanvasControl m_scrollLaneCanvas{ nullptr };
        canvasXaml::CanvasControl m_pinnedLauncherCanvas{ nullptr };
        canvasXaml::CanvasControl m_scrollLauncherCanvas{ nullptr };
        canvasXaml::CanvasControl m_editorCanvas{ nullptr };

        std::vector<std::pair<seq::ArrangeRow, xaml::UIElement>> m_pinnedHeaders{};
        std::vector<std::pair<seq::ArrangeRow, xaml::UIElement>> m_scrollHeaders{};

        // MIDI
        midi2::MidiSession m_session{ nullptr };
        std::unique_ptr<seq::SessionEngineOutput> m_output{};
        std::unique_ptr<seq::PlaybackEngine> m_engine{};
        std::unique_ptr<seq::SourceMonitor> m_sources{};
        std::shared_ptr<seq::EndpointDirectory> m_directory{ std::make_shared<seq::EndpointDirectory>() };
        std::atomic<bool> m_preparing{ false };
        std::atomic<bool> m_preparePending{ false };
        std::atomic<bool> m_endpointRefreshPending{ false };
        bool m_serviceRunning{ false };
        int64_t m_position{ 0 };
        std::unordered_map<std::wstring, seq::TrackLaunchView> m_launchViews{};

        // Armed tracks and what's recording. Echo routes are read on the service's thread.
        std::unordered_set<std::wstring> m_armed{};
        std::atomic<bool> m_recording{ false };

        struct EchoRoute
        {
            std::wstring TrackId{};
            std::wstring SourceId{};
            seq::RecordFilter Filter{};
            seq::TrackDestination Destination{};
            bool Echo{ true };
        };

        std::mutex m_inputLock{};
        std::vector<EchoRoute> m_echoRoutes{};
        std::map<std::wstring, seq::RecordingTake> m_takes{};
        int64_t m_recordStartTick{ 0 };

        // The last few minutes each armed track heard, for Capture.
        struct HeardMessage
        {
            uint64_t Timestamp{ 0 };
            std::array<uint32_t, 4> Words{};
            uint8_t WordCount{ 0 };
        };

        std::map<std::wstring, std::deque<HeardMessage>> m_heard{};
        std::atomic<uint64_t> m_lastInputTicks{ 0 };

        xaml::DispatcherTimer m_frameTimer{ nullptr };
        xaml::DispatcherTimer m_serviceTimer{ nullptr };
        xaml::DispatcherTimer m_autosaveTimer{ nullptr };
        winrt::event_token m_renderingToken{};
        winrt::event_token m_closingToken{};

        static constexpr uint32_t SampleSeed = 20266;
    };
}

namespace winrt::midisequencer::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
