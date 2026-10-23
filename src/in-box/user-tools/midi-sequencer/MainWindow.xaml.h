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
#include "DeviceProperties.h"
#include "MidiCiAppPresence.h"
#include "MidiCiProgramListQuery.h"
#include "RecentSequences.h"
#include "StudioSetup.h"
#include "TrackSound.h"
#include "EndpointDirectory.h"
#include "DawProject.h"
#include "GeneratorKinds.h"
#include "GeneratorView.h"
#include "Lyrics.h"
#include "PianoRoll.h"
#include "ClipLaneStrip.h"
#include "PatternEditor.h"
#include "SequenceGenerator.h"
#include "PlaybackEngine.h"
#include "RecordingTake.h"
#include "RemoteControl.h"
#include "RemoteEndpoint.h"
#include "SequenceEdits.h"
#include "SequenceModel.h"
#include "SequenceUndo.h"
#include "SessionEngineOutput.h"
#include "SourceMonitor.h"
#include "ThemePalette.h"
#include "ThemeXaml.h"
#include "TimelineBar.h"
#include "ThemePlates.h"
#include "ThemeCanvas.h"

namespace winrt::midisequencer::implementation
{
    namespace seq = ::midisequencer;

    // What the Sound dialog shows (MainWindowSound.cpp).
    struct SoundDialogState;

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
        void OnOpenRecentClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSaveClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSaveAsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportStandardMidiFileClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportClipFileClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportDawProjectClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportCaptureClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportPasteClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnExportClipFilesClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnExportStandardMidiFileClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnExportDawProjectClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnExportLyricsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnLyricsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSequenceInfoClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRemoteControlClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnStudioSetupClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCloseWindowClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnGoToStartClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPlayClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnStopClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRecordClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnLoopClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnMetronomeClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnMetronomeMenuClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnClockClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTempoDisplayClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
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
        void OnRazorClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnMixerToggleClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnMixerCloseClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnVerticalScrollChanged(_In_ foundation::IInspectable const& sender, _In_ primitives::RangeBaseValueChangedEventArgs const& args);


        void OnInspectorCloseClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTrackNameCommitted(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTrackNameKeyDown(_In_ foundation::IInspectable const& sender, _In_ input::KeyRoutedEventArgs const& args);
        void OnTrackPinClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSourceChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnSourceDetailsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnEchoToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRecordFromKeyboardClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnDestinationChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnDestinationDetailsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSoundClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTrySoundClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnEditorDrawClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnEditorGhostClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnEditorAutomationClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnEditorQuantizeClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTransposeClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnEditorKeyboardClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnValuesAsChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnEditorCloseClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPatternStepsChanged(_In_ controls::NumberBox const& sender, _In_ controls::NumberBoxValueChangedEventArgs const& args);
        void OnPatternStepSizeChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnPatternSwingChanged(_In_ controls::NumberBox const& sender, _In_ controls::NumberBoxValueChangedEventArgs const& args);
        void OnPatternKeepLengthClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPatternVaryClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPatternMakeNotesClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPatternHearClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPatternTemplatesOpening(_In_ foundation::IInspectable const& sender, _In_ foundation::IInspectable const& args);
        void OnGeneratorBarsChanged(_In_ controls::NumberBox const& sender, _In_ controls::NumberBoxValueChangedEventArgs const& args);
        void OnGeneratorStepSizeChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnGeneratorSameClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnGeneratorChangesClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnGeneratorSeedKeyDown(_In_ foundation::IInspectable const& sender, _In_ input::KeyRoutedEventArgs const& args);
        void OnGeneratorSeedCommitted(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
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

        // ---- the style: the theme the window takes its look from (MainWindowStyle.cpp) ----
        void LoadStyles() noexcept;
        void ApplyStyle() noexcept;
        void RefreshStyle() noexcept;
        xaml::UIElement MakeStylePicker();
        xaml::UIElement MakeTimelineBarSettings();
        xaml::UIElement MakeAutoSaveSetting();
        xaml::UIElement MakeStylePreview(_In_ themeengine::ThemeDefinition const& definition, _In_ bool dark, _In_ std::optional<themeengine::Color> tint);
        void ApplyStrips() noexcept;
        void ColorDialogs() noexcept;
        void ColorMetronome() noexcept;

        // A style's plates behind the transport, the track list's toolbar and the editor's
        // header, and behind the track headers and scene headers (MainWindowPlates.cpp).
        void SetUpPlates() noexcept;
        void ApplyPlates() noexcept;
        void RefreshPlates() noexcept;
        bool HeadersPlated() const noexcept;
        bool HeaderLedsPlated() const noexcept;
        void PlateHeaderRow(
            _In_ seq::ArrangeRow const& row,
            _In_opt_ seq::Track const* track,
            _In_ controls::Grid const& grid,
            _In_ shapes::Rectangle const& strip,
            _In_ std::vector<std::pair<primitives::ToggleButton, std::wstring_view>> const& toggles,
            _In_opt_ shapes::Ellipse const& led) noexcept;
        void PlateScene(
            _In_ controls::Button const& button,
            _In_ controls::TextBlock const& label,
            _In_ ::winrt::Microsoft::UI::Xaml::Documents::Run const& number,
            _In_ controls::FontIcon const& play) noexcept;
        void UpdateStatusBar() noexcept;
        void UpdateClockStatus() noexcept;
        void ShowMessage(_In_ winrt::hstring const& text) noexcept;
        void OnFrame() noexcept;
        void OnServiceTimer() noexcept;
        void OnWindowClosing(_In_ winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args);

        // Whether the window can close now, for the close button and File > Close alike. A take
        // being recorded goes into the sequence first. Unsaved changes are saved, or asked about;
        // the save or the answer then closes the window itself.
        bool ReadyToClose() noexcept;
        winrt::fire_and_forget AskAboutUnsavedChanges();

        // A close asked for while a save was writing goes ahead once it's done.
        void CloseIfAsked() noexcept;
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

        // A file that isn't a sequence, by its extension: a clip file, a DAWproject file, a
        // capture from MIDI Monitor or the console, or a Standard MIDI File.
        void ImportFile(_In_ std::wstring const& path) noexcept;
        void PlayOnSynth(_Inout_ seq::Track& track) noexcept;
        winrt::fire_and_forget ImportDawProjectAsync(std::wstring path);
        void AddDawProject(_In_ seq::ImportedDawProject imported, _In_ std::wstring const& path) noexcept;
        winrt::fire_and_forget ImportCaptureAsync(std::wstring path);

        // Messages copied in MIDI Monitor, as a new clip on the selected track at the playhead.
        winrt::fire_and_forget PasteMessagesAsync();
        winrt::fire_and_forget SaveAsync(bool chooseName);
        winrt::fire_and_forget AutosaveAsync();
        void RunPendingSave() noexcept;

        // One sequence per window: a window can take another sequence only while it holds an
        // untouched new one. Otherwise the other sequence opens in a window of its own.
        bool CanReplaceDocument() const noexcept;
        void OpenInNewWindow(_In_ std::wstring const& path, _In_ bool sample = false) noexcept;

        // ---- the recent sequences and the pinned ones (MainWindowRecent.cpp) ----

        // Reads the list as it is now, changes it and writes it back, one window at a time. A
        // null edit just reads it.
        std::vector<seq::RecentSequence> EditRecentSequences(_In_ std::function<void(std::vector<seq::RecentSequence>&)> const& edit) noexcept;
        void RememberRecentSequence(_In_ std::wstring const& path) noexcept;
        void OpenRecentSequence(_In_ std::wstring const& path) noexcept;
        void ShowRecentSequences();
        std::wstring ShowFileDialog(_In_ bool save, _In_ std::wstring const& defaultName, _In_ std::vector<std::pair<std::wstring, std::wstring>> const& filters, _In_ bool folders = false, _In_ std::wstring const& startFolder = {}) noexcept;
        std::wstring DocumentsFolder(_In_ std::wstring const& subfolder) noexcept;

        // Records the changes for undo, marks the sequence changed, and redraws.
        void Commit(_In_ std::wstring const& name, _In_ seq::ChangeList changes);
        void EditTracks(_In_ std::wstring const& name, _In_ std::function<void(seq::Sequence&)> const& edit);
        void DocumentChanged() noexcept;
        void Publish() noexcept;
        void Undo() noexcept;
        void Redo() noexcept;

        // A drag keeps what changed between its press and its release as one edit, so undo and
        // redo wait until it's let go.
        bool GestureActive() const noexcept;
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

        // The timeline's scroll bar and zoom (MainWindowTimelineBar.cpp, design section 31).
        void SetUpTimelineBar() noexcept;
        void PlaceTimelineBar() noexcept;
        void SetTimelineOverview(_In_ bool overview) noexcept;
        void SetTimelineBarAtTop(_In_ bool atTop) noexcept;
        seq::TimelineSpan TimelineView() noexcept;
        seq::TimelineLimits TimelineLimits() noexcept;
        seq::TimelineBarModel TimelineBarState();
        void SetTimelineView(_In_ seq::TimelineSpan const& view) noexcept;
        void ZoomTimelineStep(_In_ double factor) noexcept;
        void ZoomTimelineToFit() noexcept;
        void ZoomTimelineToSelection() noexcept;
        void OnTimelinePinch(_In_ input::ManipulationDeltaRoutedEventArgs const& args, _In_ xaml::UIElement const& canvas);
        void ScrollToTick(_In_ int64_t tick) noexcept;
        void UpdatePlayhead() noexcept;
        xaml::UIElement MakeHeaderRow(_In_ seq::ArrangeRow const& row);
        xaml::UIElement MakeLaneHeader(_In_ seq::ArrangeRow const& row);
        void SelectTrack(_In_ std::wstring const& trackId) noexcept;
        void ToggleFolder(_In_ std::wstring const& trackId) noexcept;
        void ToggleTrackFlag(_In_ std::wstring const& trackId, _In_ wchar_t flag, _In_ bool value) noexcept;
        void ShowTrackMenu(_In_ std::wstring const& trackId, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);

        // Any color for a track, with the Appearance settings' color picker (MainWindowTrackColor.cpp).
        void ShowTrackColorPicker(_In_ std::wstring const& trackId, _In_ xaml::UIElement const& anchor, _In_ std::optional<foundation::Point> point) noexcept;
        void AddTrack(_In_ bool folder) noexcept;
        void DeleteTrack(_In_ std::wstring const& trackId) noexcept;
        void BeginRenameTrack(_In_ std::wstring const& trackId) noexcept;

        void OnLanePressed(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnLaneMoved(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnLaneReleased(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnLaneWheel(_In_ input::PointerRoutedEventArgs const& args);
        void OnLauncherWheel(_In_ input::PointerRoutedEventArgs const& args, _In_ bool sceneNames);

        // The launcher scrolled sideways to x, kept between its first scene and its last, or just
        // far enough to show a scene.
        void ScrollScenes(_In_ double x) noexcept;
        void ShowScene(_In_ size_t scene) noexcept;
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
        void ShowPlacementMenu(_In_ std::wstring const& trackId, _In_ size_t placement, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point, _In_ int64_t tick = -1);

        // Merging and splitting clips and tracks (MainWindowClipEdits.cpp, SequenceMerge.h).
        std::vector<size_t> MorePlacements() const;
        void TogglePlacementInSelection(_In_ std::wstring const& trackId, _In_ size_t placement) noexcept;
        void MergeSelectedPlacements() noexcept;
        void SplitPlacementAt(_In_ std::wstring const& trackId, _In_ size_t placement, _In_ int64_t tick) noexcept;
        void SetRazor(_In_ bool on) noexcept;
        void UpdateRazor(_In_ bool pinned, _In_ foundation::Point point) noexcept;
        bool RazorPressed(_In_ bool pinned, _In_ foundation::Point point) noexcept;
        void MergeTrackInto(_In_ std::wstring const& sourceId, _In_ std::wstring const& destinationId) noexcept;
        void AddMergeTrackMenu(_In_ controls::MenuFlyout const& menu, _In_ std::wstring const& trackId);

        // Placements picked with Ctrl, besides the selected one, on the same track; they hold
        // only while that placement is still the selected one.
        std::vector<size_t> m_morePlacements{};
        size_t m_moreAnchor{ SIZE_MAX };
        std::wstring m_moreTrackId{};

        bool m_razor{ false };
        std::wstring m_razorTrackId{};
        int64_t m_razorTick{ -1 };
        void ShowLaneMenu(_In_ seq::ArrangeRow const& row, _In_ int64_t tick, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        // The track id is a copy: making the clip rebuilds the layout a caller's id may live in.
        void CreateClipAt(_In_ std::wstring trackId, _In_ int64_t tick, _In_ std::optional<seq::PatternMode> pattern = std::nullopt, _In_ std::optional<seq::GeneratorType> generator = std::nullopt) noexcept;
        void DeleteSelectedPlacement() noexcept;
        void DuplicateSelectedPlacement() noexcept;
        void MakePlacementUnique(_In_ std::wstring const& trackId, _In_ size_t placement) noexcept;
        void AddTagAt(_In_ std::wstring const& trackId, _In_ int64_t tick) noexcept;
        void RenameClip(_In_ std::wstring const& clipId) noexcept;
        void AddScene() noexcept;
        void DeleteScene(_In_ size_t scene) noexcept;
        void LaunchScene(_In_ size_t scene) noexcept;
        void LaunchSlot(_In_ std::wstring const& trackId, _In_ size_t scene) noexcept;
        void CreateClipInSlot(_In_ std::wstring const& trackId, _In_ size_t scene, _In_ std::optional<seq::PatternMode> pattern = std::nullopt, _In_ std::optional<seq::GeneratorType> generator = std::nullopt) noexcept;
        void PlaceSlotClipOnTimeline(_In_ std::wstring const& trackId, _In_ size_t scene) noexcept;
        void DeleteSlotClip(_In_ std::wstring const& trackId, _In_ size_t scene) noexcept;
        void ShowSlotMenu(_In_ std::wstring const& trackId, _In_ size_t scene, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        std::optional<std::pair<std::wstring, size_t>> HitTestSlot(_In_ bool pinned, _In_ foundation::Point point) noexcept;

        // The scene under a point on a launcher canvas, by its distance from the canvas's left edge.
        std::optional<size_t> SceneAtLauncherX(_In_ double x) const noexcept;
        void ShowTextPrompt(_In_ winrt::hstring const& title, _In_ winrt::hstring const& initial, _In_ std::function<void(std::wstring const&)> done);
        void OnArrangeSizeChanged() noexcept;
        void SetLauncherVisible(_In_ bool visible) noexcept;

        // ---- sizing, selecting and dragging in the arrangement (MainWindowArrangeDrag.cpp) ----
        void InitializeArrangeSplitters();
        void ApplyArrangeColumns() noexcept;
        void OnColumnSplitterPressed(_In_ controls::Grid const& splitter, _In_ input::PointerRoutedEventArgs const& args);
        void OnColumnSplitterMoved(_In_ controls::Grid const& splitter, _In_ input::PointerRoutedEventArgs const& args);
        void OnColumnSplitterReleased(_In_ controls::Grid const& splitter);
        void SetColumnWidth(_In_ bool header, _In_ double width, _In_ bool save) noexcept;

        // A row's height: a grip along the bottom of its header, and what it changes.
        void AddRowGrip(_In_ controls::Grid const& header, _In_ seq::ArrangeRow const& row);
        void OnHeaderCanvasMoved(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnHeaderCanvasReleased(_In_ bool pinned, _In_ bool canceled);
        void RelayoutRows() noexcept;
        void SetTracksHeight(_In_ std::vector<std::wstring> const& trackIds, _In_ uint16_t height) noexcept;
        void KeepViewChange() noexcept;

        // Several tracks can be selected: Ctrl+click adds or takes one away, Shift+click adds a run.
        bool IsTrackSelected(_In_ std::wstring_view trackId) const noexcept;
        std::vector<std::wstring> SelectedTrackIds() const;
        void ClickTrack(_In_ std::wstring const& trackId, _In_ bool control, _In_ bool shift) noexcept;
        void RefreshHeaderBackgrounds() noexcept;

        // Dragging a track's header moves it, and the selected tracks with it.
        void OnHeaderPressed(_In_ std::wstring const& trackId, _In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnHeaderMoved(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void UpdateTrackDrop(_In_ foundation::Point inArrange) noexcept;
        void FinishTrackDrag(_In_ bool drop) noexcept;

        // Dragging clips: on the timeline, between tracks, between launcher slots, and from one to
        // the other. Ctrl copies.
        struct ArrangeSpot
        {
            enum class Area : uint8_t { None, Timeline, Launcher };

            Area Where{ Area::None };
            bool Pinned{ false };
            foundation::Point Position{};
        };

        ArrangeSpot ArrangeSpotAt(_In_ foundation::Point inArrange) noexcept;
        void BeginSlotDrag(_In_ bool pinned, _In_ std::wstring const& trackId, _In_ size_t scene, _In_ input::PointerRoutedEventArgs const& args);
        void OnLauncherMoved(_In_ bool pinned, _In_ input::PointerRoutedEventArgs const& args);
        void OnLauncherReleased(_In_ bool pinned, _In_ bool canceled);

        // With keepTrack (Shift held while dragging), the clip stays on the track it came from and
        // only the pointer's left and right movement counts.
        void UpdateClipDrop(_In_ foundation::Point inArrange, _In_ bool copy, _In_ bool keepTrack) noexcept;
        void FinishClipDrag(_In_ bool drop) noexcept;
        void StartDragScroll() noexcept;
        void OnDragScrollTick() noexcept;

        // Tags: dragged along their row, renamed, colored and deleted.
        struct TagHit
        {
            std::wstring TrackId{};
            size_t Index{ SIZE_MAX };
            bool Valid() const noexcept { return Index != SIZE_MAX; }
        };

        TagHit HitTestTag(_In_ seq::ArrangeRow const& row, _In_ float rowTop, _In_ foundation::Point point) noexcept;
        void BeginTagDrag(_In_ bool pinned, _In_ TagHit const& hit, _In_ input::PointerRoutedEventArgs const& args);
        void OnTagDragMoved(_In_ input::PointerRoutedEventArgs const& args);
        void FinishTagDrag() noexcept;
        void ShowTagMenu(_In_ TagHit const& hit, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        void EditTag(_In_ TagHit const& hit, _In_ std::wstring const& undoName, _In_ std::function<void(std::vector<seq::Tag>&, size_t)> const& edit);
        void RenameTag(_In_ TagHit const& hit);

        // The loop on the ruler: its ends and its middle can be dragged.
        enum class LoopGrab : uint8_t { None, Start, End, Whole };
        LoopGrab HitTestLoop(_In_ foundation::Point point) const noexcept;

        // ---- the mixer (MainWindowMixer.cpp) ----
        bool MixerShown() noexcept;
        void ShowMixer(_In_ bool show) noexcept;
        void RebuildMixer() noexcept;
        void RefreshMixer() noexcept;
        void UpdateMixer(_In_ int64_t tick) noexcept;
        xaml::UIElement MakeMixerStrip(_In_ seq::Track const& track);
        std::optional<uint32_t> MixerValue(_In_ seq::Track const& track, _In_ uint8_t controller, _In_ int64_t tick, _Out_ bool& followsLane) const;
        void OnMixerValueChanged(_In_ std::wstring const& trackId, _In_ uint8_t controller, _In_ uint32_t value) noexcept;

        // A move goes to the device no more often than its automation interval from MIDI
        // Settings. One that comes sooner waits, and the newest one waiting is always sent.
        void SendMixerValue(_In_ seq::Track const& track, _In_ uint8_t controller, _In_ uint32_t value) noexcept;
        void SendWaitingMixerValues() noexcept;
        void ScheduleMixerSend(_In_ std::chrono::steady_clock::time_point due) noexcept;

        // A mixer change waits for its control to rest before it's kept. Anything else that
        // changes the sequence, undoes or saves puts it on the history first (RecordMixerEdit),
        // so the history stays in the order things were done. CommitMixerEdit also shows it.
        bool RecordMixerEdit() noexcept;
        void CommitMixerEdit() noexcept;

        // Undo and redo show what they bring back, even on a control that was just moved.
        void ReleaseMixerControls() noexcept;
        void UpdateLeds() noexcept;
        int64_t SnapGrid() const noexcept;
        std::wstring NextClipName(_In_ wchar_t const* stemKey = L"DefaultClipName") const;
        media::SolidColorBrush BrushFor(_In_ seq::Color color) const;
        media::SolidColorBrush HeaderBackground(_In_ seq::ArrangeRow const& row) const;
        media::Brush ThemeBrush(_In_ wchar_t const* key);

        // ---- the clip editor (MainWindowEditor.cpp) ----
        // Copies of the ids: words still being typed are kept first, which builds the layout again.
        void OpenClipInEditor(_In_ std::wstring clipId, _In_ std::wstring trackId) noexcept;
        void CloseEditor() noexcept;
        void RefreshEditor() noexcept;
        void UpdateEditorHeader() noexcept;
        void UpdateNoteInspector() noexcept;
        void ApplyRollEdit(_In_ std::optional<seq::RollEdit> edit) noexcept;
        void OnEditorPressed(_In_ input::PointerRoutedEventArgs const& args);
        void OnEditorMoved(_In_ input::PointerRoutedEventArgs const& args);

        // The resize arrows over the end of a note, which a drag makes longer or shorter.
        void ShowEditorSizing(_In_ bool sizing) noexcept;
        void OnEditorReleased(_In_ input::PointerRoutedEventArgs const& args);
        void OnEditorWheel(_In_ input::PointerRoutedEventArgs const& args);
        void OnEditorKeyDown(_In_ input::KeyRoutedEventArgs const& args);
        void InvalidateEditor() noexcept;
        seq::Track const* EditorTrack() const noexcept;

        // A notes clip's own automation, in lanes under the piano roll (MainWindowClipLanes.cpp).
        void DrawNotesEditor(_In_ canvas::CanvasDrawingSession const& ds, _In_ float width, _In_ float height);

        // The piano roll's ghost notes (MainWindowGhostNotes.cpp): worked out again only when the
        // sequence, the clip or where it's placed changes.
        void UpdateGhostNotes() noexcept;
        std::wstring m_ghostKey{};
        float NotesEditorRollHeight() const noexcept;
        bool OnClipLanesPressed(_In_ input::PointerRoutedEventArgs const& args, _In_ foundation::Point position, _In_ bool right, _In_ bool control, _In_ bool doubleClick);
        bool OnClipLanesMoved(_In_ foundation::Point position);
        bool OnClipLanesReleased(_In_ foundation::Point position);
        bool DeleteSelectedClipLanePoint() noexcept;
        bool OnClipLanesKeyDown(_In_ winrt::Windows::System::VirtualKey key, _In_ bool shift, _In_ bool alt);
        void OnEditorContextRequested(_In_ input::ContextRequestedEventArgs const& args);
        void AnnounceClipLanePoint(_In_ seq::Clip const& clip, _In_ size_t lane, _In_ size_t point);

        // ---- the timeline and launcher from the keyboard (MainWindowKeyboard.cpp) ----
        void HookArrangeKeyboard();
        std::vector<seq::ArrangeRow const*> KeyboardRows(_In_ bool tracksOnly) const;
        void ShowRowForKeyboard(_In_ seq::ArrangeRow const& row);
        foundation::Point RowPoint(_In_ seq::ArrangeRow const& row, _In_ double x) const noexcept;
        void OnTimelineKeyDown(_In_ input::KeyRoutedEventArgs const& args);
        void OnLauncherKeyDown(_In_ input::KeyRoutedEventArgs const& args);
        void ShowKeyboardMenu(_In_ bool launcher);
        void AnnounceKeyboardCursor(_In_ bool launcher);
        void DrawKeyboardFocus(_In_ canvas::CanvasDrawingSession const& ds, _In_ canvasXaml::CanvasControl const& canvas);

        // ---- pattern clips (MainWindowPattern.cpp) ----
        bool EditorShowsPattern() const noexcept;
        void InitializePatternEditor() noexcept;
        void UpdatePatternToolbar() noexcept;
        void UpdatePatternInspector() noexcept;
        void UpdatePatternFooter() noexcept;
        void ApplyPatternEdit(_In_ std::optional<seq::PatternEdit> edit) noexcept;
        void EditPattern(_In_ std::wstring const& name, _In_ std::function<void(seq::PatternSettings&)> const& edit) noexcept;
        void EditSelectedStep(_In_ std::wstring const& name, _In_ std::function<void(seq::PatternStep&)> const& edit) noexcept;

        // One step of the open pattern, if it's still the clip named.
        void EditStep(_In_ std::wstring const& clipId, _In_ size_t lane, _In_ uint16_t index, _In_ std::wstring const& name, _In_ std::function<void(seq::PatternStep&)> const& edit) noexcept;
        std::wstring PatternLaneName(_In_ seq::PatternLane const& lane, _In_ seq::PatternMode mode) const;
        void ShowPatternLaneMenu(_In_ size_t lane, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        void AddPatternLane() noexcept;
        void VaryPattern() noexcept;
        void MakeNotesFromPattern() noexcept;
        void OnPatternPressed(_In_ input::PointerRoutedEventArgs const& args);
        void OnPatternMoved(_In_ input::PointerRoutedEventArgs const& args);
        void OnPatternReleased(_In_ input::PointerRoutedEventArgs const& args);
        bool OnPatternKeyDown(_In_ winrt::Windows::System::VirtualKey key, _In_ bool shift) noexcept;

        // ---- starter patterns, and hearing steps as they go on (MainWindowPatternTemplates.cpp) ----
        void UsePatternTemplate(_In_ std::wstring const& id) noexcept;
        void HearPatternStep(_In_ uint8_t note, _In_ uint16_t velocity) noexcept;

        // ---- generator clips (MainWindowGenerator.cpp) ----
        bool EditorShowsGenerator() const noexcept;
        void InitializeGeneratorEditor() noexcept;
        void UpdateGeneratorToolbar() noexcept;
        void UpdateGeneratorPanel() noexcept;
        void UpdateGeneratorFooter() noexcept;
        void UpdateGeneratorInspector() noexcept;
        void EditGenerator(_In_ std::wstring const& name, _In_ std::function<void(seq::GeneratorSettings&)> const& edit) noexcept;
        void SetGeneratorType(_In_ seq::GeneratorType type) noexcept;
        void CommitGeneratorSeed() noexcept;
        seq::Color ClipColor(_In_ seq::Clip const& clip) const;

        // The layouts of comp 4, and the drawings in them: a Euclidean rhythm's rings and what it
        // plays, and a chance melody's first passes. The rings turn when dragged round.
        void AddEuclidSettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ std::function<void(std::function<void(seq::GeneratorSettings&)>)> const& edit);
        void AddMelodySettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ std::function<void(std::function<void(seq::GeneratorSettings&)>)> const& edit);
        canvasXaml::CanvasControl MakeGeneratorCanvas(_In_ std::function<void(canvas::CanvasDrawingSession const&, float, float)> draw);
        controls::Slider MakeGeneratorSlider(_In_ winrt::hstring const& name, _In_ double maximum, _In_ double value, _In_ controls::TextBlock const& readout, _In_ std::function<void(double)> commit);
        void FitGeneratorPanel() noexcept;
        void UpdatePassCaption() noexcept;
        void RefreshRingRows() noexcept;
        void OnRingPressed(_In_ input::PointerRoutedEventArgs const& args);
        void OnRingMoved(_In_ input::PointerRoutedEventArgs const& args);
        void OnRingReleased();
        void OnRingWheel(_In_ input::PointerRoutedEventArgs const& args);

        // Design section 28's generators: the pieces their panels share (MainWindowGeneratorKit.cpp),
        // and a panel for each with a drawing to play with (MainWindowLife.cpp, MainWindowLearn.cpp,
        // MainWindowEvolve.cpp and MainWindowStreams.cpp).
        using GeneratorEdit = std::function<void(std::function<void(seq::GeneratorSettings&)>)>;

        void BuildGeneratorTypePicker();
        controls::NumberBox MakeGeneratorNumberBox(_In_ winrt::hstring const& name, _In_ double minimum, _In_ double maximum, _In_ double value, _In_ std::optional<bool> note);
        controls::MenuFlyoutSubItem MakeNewGeneratorMenu(_In_ winrt::hstring const& text, _In_ std::function<void(seq::GeneratorType)> const& create);
        controls::StackPanel AddGeneratorSection(_In_ controls::StackPanel const& panel, _In_ wchar_t const* titleKey);
        controls::StackPanel MakeChipRow(_In_ std::vector<winrt::hstring> const& labels, _In_ size_t selected, _In_ winrt::hstring const& name, _In_ std::function<void(size_t)> const& choose);
        primitives::ToggleButton MakeChip(_In_ winrt::hstring const& label, _In_ bool on, _In_ std::function<void(bool)> const& toggled);
        void AppendSeparateChip(_In_ controls::StackPanel const& row, _In_ primitives::ToggleButton const& chip);
        void AddKeyAndRange(_In_ controls::StackPanel const& panel, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit, _In_ bool rules);
        void AddKeyRules(_In_ controls::StackPanel const& panel, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);

        // How loud it plays: the ways it can set each note's velocity, and the range they play in
        // over the pass's notes (MainWindowVelocity.cpp).
        void AddVelocity(_In_ controls::StackPanel const& panel, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);
        void SetGeneratorVelocity(_In_ uint16_t low, _In_ uint16_t high) noexcept;

        // Listening to another clip or a track (MainWindowListen.cpp, design section 29): the list
        // of sources with Learn and Keep listening, learning from one, and the settings a pass
        // shown in the editor plays with, after what it heard. Life and Evolve carry what they
        // heard before every pass instead; the hearing they're drawn with uses the clip, so it's
        // used while the clip is.
        void AddListenSource(_In_ controls::StackPanel const& panel, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);
        void LearnFromSource(_In_ seq::ListenKind kind, _In_ std::wstring const& id) noexcept;
        seq::GeneratorSettings ShownPassSettings(_In_ seq::Clip const& clip, _In_ uint64_t pass) const;
        seq::PassHearing ShownPassHearing(_In_ seq::Clip const& clip, _In_ uint64_t pass) const;
        int64_t ListenPassStart(_In_ seq::Clip const& clip, _In_ uint64_t pass) const noexcept;
        controls::Border FrameGeneratorDrawing(_In_ xaml::FrameworkElement const& drawing);
        controls::Button MakeGeneratorNotesButton(_In_ std::wstring const& clipId);
        controls::Grid MakeGeneratorLayout(_In_ xaml::FrameworkElement const& drawing, _In_ controls::StackPanel const& settings, _In_ controls::StackPanel const& side);
        double GeneratorDrawingWidth(_In_ double least, _In_ double most) const noexcept;
        void EditGeneratorInPlace(_In_ std::function<void(seq::GeneratorSettings&)> const& change) noexcept;

        // The panel built again once the pointer event under way is over, if the clip is still
        // open: built inside it, what replaces the control under the pointer loses the next press.
        void QueueGeneratorPanelUpdate() noexcept;
        uint64_t ShownGeneratorPass(_In_ seq::Clip const& clip) const noexcept;
        int64_t PlayingGeneratorStep(_In_ seq::Clip const& clip) const noexcept;
        void PickGeneratorPass(_In_ uint64_t pass) noexcept;

        void AddLifeSettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);
        void AddLearnSettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);
        void AddWordsSettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);
        void AddEvolveSettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);
        void AddInfinitySettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);
        void AddLSystemSettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);
        void AddRingingSettings(_In_ controls::StackPanel const& body, _In_ seq::Clip const& clip, _In_ GeneratorEdit const& edit);

        // The arpeggiator (MainWindowArpeggiator.cpp): its settings, Make notes, and the keys
        // played on the sources of tracks it plays from.
        void AddArpeggiatorSettings(_In_ controls::StackPanel const& body, _In_ seq::GeneratorSettings const& generator, _In_ std::function<void(std::function<void(seq::GeneratorSettings&)>)> const& edit);
        void MakeNotesFromArpeggiator() noexcept;
        void UpdateArpeggiatorRoutes() noexcept;
        std::vector<std::wstring> FeedArpeggiators(_In_ std::wstring const& endpointId, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;
        void RecordArpeggiatedNotes() noexcept;

        // The launcher's state from the engine, and what waits for MIDI to start
        // (MainWindowLaunchState.cpp). WaitForMidi is true when it kept the action for later.
        void RefreshLaunchViews() noexcept;
        bool WaitForMidi(_In_ std::function<void()> action) noexcept;
        void RunWaitingForMidi() noexcept;
        std::vector<std::function<void()>> m_waitingForMidi{};

        // What armed tracks play from launched clips, into their takes (MainWindowRecordPlayed.cpp).
        void SetRecordedTracks(_In_ bool recording) noexcept;
        void RecordPlayedNotes(_In_ int64_t until = std::numeric_limits<int64_t>::max()) noexcept;

        // ---- automation and the tempo map (MainWindowAutomation.cpp) ----
        // The lane functions take the id of the track or the clip the lanes belong to.
        std::vector<seq::AutomationLane>* LanesOwnedBy(_In_ std::wstring const& ownerId) noexcept;
        std::wstring LaneName(_In_ seq::AutomationLane const& lane) const;
        std::wstring LaneDescription(_In_ seq::Track const& track, _In_ seq::AutomationLane const& lane) const;
        std::wstring LaneValueText(_In_ seq::AutomationLane const& lane, _In_ uint32_t value) const;
        std::optional<uint32_t> PlayingLaneValue(_In_ seq::Track const& track, _In_ seq::AutomationLane const& lane, _In_ int64_t tick) const;
        void UpdateLaneChips(_In_ int64_t tick) noexcept;
        void ToggleLanes(_In_ std::wstring const& trackId) noexcept;
        void ShowAddLaneDialog(_In_ std::wstring const& trackId);
        void EditLanes(_In_ std::wstring const& trackId, _In_ std::wstring const& name, _In_ std::function<void(std::vector<seq::AutomationLane>&)> const& edit);
        void RenameLane(_In_ std::wstring const& trackId, _In_ std::wstring const& laneId);
        void ShowLaneRowMenu(_In_ std::wstring const& trackId, _In_ std::wstring const& laneId, _In_ std::optional<seq::LanePoint> addAt, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        void ShowLanePointMenu(_In_ std::wstring const& trackId, _In_ std::wstring const& laneId, _In_ size_t index, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        void ShowTempoPointMenu(_In_ size_t index, _In_ xaml::UIElement const& anchor, _In_ foundation::Point point);
        void AddTempoMenuItems(_In_ controls::MenuFlyout const& menu, _In_ int64_t tick);

        // The lyrics track (MainWindowLyrics.cpp).
        xaml::UIElement MakeLyricsHeader(_In_ seq::ArrangeRow const& row);
        void AddLyricsMenuItems(_In_ controls::MenuFlyout const& menu);
        void ShowLyricsEditor();
        void ApplyLyrics(_In_ std::optional<seq::LyricsTrack> lyrics, _In_ std::wstring const& undoName);
        void UpdateLyricsBar() noexcept;

        // About this sequence (MainWindowProvenance.cpp).
        void ShowSequenceInfo();
        void StampProvenance() noexcept;
        void PromptForTempo(_In_ int64_t tick, _In_ std::optional<size_t> index);
        void PromptForMeter(_In_ int64_t bar);
        void ShowTempoFlyout(_In_ xaml::FrameworkElement const& anchor);
        void DeleteSelectedAutomationPoint() noexcept;
        void ClearAutomationSelection() noexcept;
        int64_t TickAtLaneX(_In_ double x) const noexcept;
        size_t HitTestLanePoint(_In_ seq::AutomationLane const& lane, _In_ float rowTop, _In_ float rowHeight, _In_ foundation::Point point) const noexcept;
        size_t HitTestTempoPoint(_In_ float rowTop, _In_ float rowHeight, _In_ foundation::Point point) const noexcept;
        bool OnAutomationPressed(_In_ bool pinned, _In_ seq::ArrangeRow const& row, _In_ input::PointerRoutedEventArgs const& args);
        bool OnTempoPressed(_In_ bool pinned, _In_ seq::ArrangeRow const& row, _In_ input::PointerRoutedEventArgs const& args);
        void OnPointDragMoved(_In_ input::PointerRoutedEventArgs const& args);
        void OnPointDragReleased();
        bool OnAutomationRightTapped(_In_ bool pinned, _In_ seq::ArrangeRow const& row, _In_ input::RightTappedRoutedEventArgs const& args);

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

        // Recording into a launcher slot: from the next launch point for the record length, then
        // the take loops straight away.
        enum class SlotTakeEnd : uint8_t { ReachedEnd, Early, Stopped };
        void StartSlotRecording(_In_ std::wstring const& trackId, _In_ size_t scene) noexcept;
        void UpdateSlotRecording() noexcept;
        void FinishSlotRecording(_In_ SlotTakeEnd how) noexcept;
        std::optional<seq::SlotRecording> SlotRecordingView() const;
        void OnSourceMessage(_In_ std::wstring const& endpointId, _In_ uint64_t timestamp, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;
        void OnFollowedClock(_In_ uint64_t timestamp, _In_ uint32_t word0) noexcept;
        void OnFollowedTransport() noexcept;
        void UpdateEchoRoutes() noexcept;
        // Sends in the protocol the destination group speaks. A timestamp of 0 sends at once; a
        // later one is held by the service until then.
        void SendNow(_In_ seq::TrackDestination const& destination, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount, _In_ uint64_t timestamp = 0) noexcept;
        void AuditionNote(_In_ uint8_t note, _In_ bool on) noexcept;
        void LaunchKeyboard(_In_ std::wstring const& endpointId, _In_ uint8_t group, _In_ int8_t channel, _In_ std::wstring const& programsFromId = {}, _In_ uint8_t programsGroup = 0) noexcept;
        void ShowMetronomeFlyout();

        // ---- clock and sync (MainWindowClock.cpp) ----
        void ShowClockDialog();

        // ---- what Windows MIDI Services says about each device (MainWindowDevices.cpp) ----
        void ShowDeviceDetails(_In_ seq::EndpointRef const& endpoint, _In_ xaml::FrameworkElement const& anchor) noexcept;
        void AddDeviceProperties(_In_ controls::StackPanel const& host, _In_ controls::TextBlock const& kind, _In_ seq::DeviceProperties const& properties);
        std::vector<std::wstring> DeviceUses(_In_ seq::EndpointRef const& endpoint) const;
        controls::Image DevicePictureFor(_In_ seq::EndpointRef const& endpoint, _In_ double size) const;
        void SetDeviceToolTip(_In_ xaml::DependencyObject const& element, _In_ seq::EndpointRef const& endpoint) const;
        winrt::hstring DestinationToolTip(_In_ seq::Track const& track) const;

        // How often a lane's ramps go to the track's device, and why.
        winrt::hstring LaneIntervalToolTip(_In_ seq::Track const& track) const;
        void UpdateDeviceDescriptions() noexcept;
        void OpenMidiSettings() noexcept;

        // MIDI Monitor on a device, or on one of its groups. -1 is all of them.
        void LaunchMonitor(_In_ std::wstring const& endpointId, _In_ int32_t group) noexcept;

        // The same for two saved endpoints that mean the same device here.
        std::wstring EndpointKeyOf(_In_ seq::EndpointRef const& endpoint) const;

        // ---- what each device gets besides its notes (MainWindowStudio.cpp) ----
        void ShowStudioSetup();

        // ---- groups turned off in Studio setup (MainWindowPorts.cpp) ----
        std::vector<seq::PortSwitch> SavedPortSwitches() const;

        // The groups of a device the sequencer doesn't play to, or doesn't record from, one bit each.
        uint16_t GroupsTurnedOff(_In_ seq::EndpointRef const& endpoint, _In_ bool playTo) const;

        // The groups a device sends on, one bit each: 0 when it isn't here for SourceGroupMask,
        // the first group for AnySourceGroups.
        uint16_t SourceGroupMask(_In_ seq::EndpointRef const& endpoint) const;
        uint16_t AnySourceGroups(_In_ seq::EndpointRef const& endpoint) const;
        bool DestinationTurnedOff(_In_ seq::TrackDestination const& destination) const;

        // True only when every group the track records from is turned off.
        bool SourceTurnedOff(_In_ seq::TrackSource const& source) const;

        // Leaves out the devices whose every group in use is turned off, and tells the output and
        // the sources which groups to drop. The maps say which groups each one is used on.
        void LeaveOutGroupsOff(
            _Inout_ std::vector<seq::EndpointRef>& destinations,
            _In_ std::map<std::pair<std::wstring, std::wstring>, uint16_t> const& sendsTo,
            _Inout_ std::vector<std::wstring>& sources,
            _In_ std::map<std::wstring, uint16_t> const& listensTo);

        // ---- the sound a track picks before it plays (MainWindowSound.cpp) ----
        bool IsGeneralMidiDevice(_In_ seq::EndpointRef const& endpoint) const;

        // The track's sound by name, or empty when it doesn't choose one.
        std::wstring SoundDisplayName(_In_ seq::Track const& track) const;
        void UpdateSoundSection() noexcept;
        void ShowSoundDialog();
        void FillSoundList(_Inout_ SoundDialogState& state);
        void ShowSoundCategories(_Inout_ SoundDialogState& state);
        void ShowSoundNumbers(_Inout_ SoundDialogState& state);
        void TrySound(_Inout_ SoundDialogState& state);
        winrt::fire_and_forget AskDeviceForSounds(_In_ std::shared_ptr<SoundDialogState> state);
        void OnDeviceSounds(_In_ std::wstring const& key, _In_ midiapp::ProgramListResult result, _In_ std::vector<midiapp::ProgramListEntry> entries) noexcept;

        // This app's MIDI-CI presence on a device, made the first time it's asked anything. Blocks
        // on the service: never call it on the UI thread.
        winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquirySession CiSessionFor(
            _In_ midi2::MidiSession const& midiSession, _In_ std::wstring const& endpointId, _In_ uint8_t group);
        void CloseCiLinks() noexcept;

        // ---- remote control and MIDI Glass (MainWindowRemote.cpp) ----
        winrt::fire_and_forget StartRemoteControlAsync();
        // Closing waits on the MIDI service: in the background when it's turned off, and right away
        // when the window closes, so it's gone before the process is.
        void StopRemoteControl(_In_ bool inBackground = false) noexcept;
        void OnRemoteMessage(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;
        seq::RemoteState CurrentRemoteState();
        void UpdateRemoteFeedback() noexcept;
        void ShowRemoteControlDialog();
        winrt::fire_and_forget MakeGlassLayoutAsync();

        // Another tool installed beside this one, or empty when it isn't.
        std::filesystem::path SiblingToolPath(_In_ std::wstring const& executable) const;
        seq::LaunchQuantize LaunchQuantize() const noexcept;
        std::wstring DescribeDestination(_In_ seq::Track const& track) const;
        std::wstring DescribeSource(_In_ seq::Track const& track) const;

        // ---- the track inspector (MainWindowTrack.cpp) ----
        void ShowInspector(_In_ std::wstring const& trackId) noexcept;
        void FollowSelectionInInspector() noexcept;
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

        // A save closing the window couldn't write, so closing asks rather than trying again.
        bool m_closeSaveFailed{ false };

        // The window was asked to close while a save was writing.
        bool m_closeRequested{ false };

        // One save at a time: a save asked for while another writes waits for it. 1 saves, 2 asks
        // for a name.
        bool m_saving{ false };
        uint8_t m_savePending{ 0 };

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

        // Imported tracks waiting for the synth, when an import came before the list of devices.
        std::vector<std::wstring> m_awaitingSynth{};

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
        winrt::hstring m_textStoppedFromLauncher{};
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

        // A clip's end, dragged on the timeline to change its length.
        enum class LaneDrag : uint8_t { None, Resize };
        LaneDrag m_laneDrag{ LaneDrag::None };
        bool m_lanePinned{ false };
        foundation::Point m_dragStart{};
        int64_t m_dragOriginalLength{ 0 };
        int64_t m_dragLength{ 0 };
        std::vector<seq::Track> m_dragTracksBefore{};
        int64_t m_loopDragStart{ 0 };
        bool m_rulerDragging{ false };

        bool m_splitterDragging{ false };
        double m_splitterStartY{ 0 };
        double m_splitterStartHeight{ 0 };

        // How wide the track list and the clip launcher are, and dragging the bars between them.
        double m_headerWidth{ seq::AppSettings::DefaultHeaderWidth };
        double m_launcherWidth{ seq::AppSettings::DefaultLauncherWidth };
        bool m_columnDragging{ false };
        bool m_columnDragHeader{ true };
        double m_columnDragStartX{ 0 };
        double m_columnDragStartWidth{ 0 };

        // Dragging a row's grip. Tempo is the Tempo and meter row; otherwise TrackId's row.
        struct RowResize
        {
            bool Active{ false };
            bool Moved{ false };
            bool Tempo{ false };
            std::wstring TrackId{};
            double StartY{ 0 };
            double StartHeight{ 0 };
        };

        RowResize m_rowResize{};

        // Every selected track, m_selectedTrackId among them, and where Shift+click runs from.
        std::vector<std::wstring> m_selectedTrackIds{};
        std::wstring m_selectionAnchorId{};

        // A track header being dragged, and where its tracks would go: into FolderId (the top level
        // when empty), before BeforeId (the end when empty). IntoFolderId is the folder whose own
        // row it's over, drawn with a box rather than a line.
        struct TrackDrag
        {
            bool Pressed{ false };
            bool Active{ false };
            bool Pinned{ false };
            foundation::Point Start{};
            std::vector<std::wstring> TrackIds{};

            bool Valid{ false };
            std::wstring FolderId{};
            std::wstring BeforeId{};
            std::wstring IntoFolderId{};
        };

        TrackDrag m_trackDrag{};
        shapes::Rectangle m_dropLine{ nullptr };
        shapes::Rectangle m_dropBox{ nullptr };

        // A clip being dragged from the timeline or from a launcher slot.
        struct ClipDrag
        {
            bool Pressed{ false };
            bool Active{ false };
            bool FromLauncher{ false };
            bool Pinned{ false };
            foundation::Point Start{};
            std::wstring ClipId{};
            std::wstring FromTrackId{};
            size_t FromPlacement{ SIZE_MAX };
            size_t FromSlot{ SIZE_MAX };

            // Ticks from the clip's start to where it was picked up, and how long it plays.
            int64_t GrabOffset{ 0 };

            // Pressed with Ctrl: let go without moving, it's added to or taken from the selection.
            bool ToggleOnClick{ false };
            int64_t Length{ 0 };

            seq::ClipDragView View{};
        };

        ClipDrag m_clipDrag{};
        xaml::DispatcherTimer m_dragScrollTimer{ nullptr };
        foundation::Point m_dragPoint{};
        bool m_dragCopy{ false };
        bool m_dragKeepTrack{ false };

        // A tag being dragged along its row.
        struct TagDrag
        {
            bool Active{ false };
            bool Moved{ false };
            bool Pinned{ false };
            TagHit Tag{};
            foundation::Point Start{};
            int64_t OriginalTick{ 0 };
            int64_t Tick{ 0 };
        };

        TagDrag m_tagDrag{};

        // Dragging the loop's ends or its middle on the ruler.
        LoopGrab m_loopGrab{ LoopGrab::None };
        int64_t m_loopGrabStart{ 0 };
        int64_t m_loopGrabEnd{ 0 };

        // When a press last added an automation point or a tempo change, so the second press of a
        // double click doesn't take it away again.
        std::chrono::steady_clock::time_point m_pointAddedAt{};

        // Which cursor each lane canvas shows: -1 not set yet, 0 the arrow, 1 for a clip's end.
        std::array<int8_t, 2> m_laneCursorSizing{ -1, -1 };

        // The mixer: a strip for each track, and each control on it.
        struct MixerControl
        {
            std::wstring TrackId{};
            uint8_t Controller{ 0 };
            controls::Slider Slider{ nullptr };
            controls::TextBlock Value{ nullptr };
            xaml::UIElement LaneMark{ nullptr };

            // When it was last moved by hand, so its lane doesn't move it back meanwhile.
            std::chrono::steady_clock::time_point TouchedAt{};
        };

        struct MixerStripView
        {
            std::wstring TrackId{};
            controls::TextBlock Name{ nullptr };
            controls::TextBlock Destination{ nullptr };
            shapes::Rectangle Bar{ nullptr };
            primitives::ToggleButton Mute{ nullptr };
            primitives::ToggleButton Solo{ nullptr };
            primitives::ToggleButton Write{ nullptr };

            // The outline that shows the track is selected.
            controls::Border Frame{ nullptr };
            bool Selected{ false };
            bool SelectionShown{ false };

            // The meter's segments, from the bottom, and how many are lit.
            std::vector<shapes::Rectangle> Meter{};
            int32_t MeterLit{ -1 };
        };

        std::vector<MixerControl> m_mixerControls{};
        std::vector<MixerStripView> m_mixerStrips{};
        std::vector<std::wstring> m_mixerTrackIds{};
        bool m_mixerUpdating{ false };

        // The strips' look, from MainWindowMixerStrip.cpp: one template for every knob and fader,
        // and when the meters last looked at the engine.
        controls::ControlTemplate m_mixerSliderTemplate{ nullptr };
        uint64_t m_meterFrom{ 0 };

        // The name plate's color, with black or white text, whichever reads better on it.
        void ColorMixerPlate(_In_ MixerStripView const& view, _In_ uint32_t rgb);
        void UpdateMixerSelection() noexcept;

        // Each meter shows how hard its track's notes play, from the engine's note ons and offs.
        void UpdateMixerMeters() noexcept;

        // Tracks whose mixer moves are written into lanes while the sequence plays. Not saved.
        std::unordered_set<std::wstring> m_automationArmed{};

        // A mixer change being made: kept as one edit once the control rests.
        struct MixerEdit
        {
            enum class Kind : uint8_t { None, Startup, Point, Write };

            Kind What{ Kind::None };
            std::wstring TrackId{};
            uint8_t Controller{ 0 };
            std::vector<seq::Track> TracksBefore{};

            // The track as the last move left it. A change only ever touches its own track, so
            // this and TracksBefore are the whole edit, whatever else changed the tracks since.
            std::optional<seq::Track> TrackAfter{};

            // Writing: the lane, its points before, and what's been written, by tick, and when the
            // last point was written.
            std::wstring LaneId{};
            std::vector<seq::LanePoint> PointsBefore{};
            std::map<int64_t, uint32_t> Drawn{};
            std::chrono::steady_clock::time_point WrittenAt{};
            std::chrono::steady_clock::time_point PublishedAt{};
        };

        MixerEdit m_mixerEdit{};
        xaml::DispatcherTimer m_mixerCommitTimer{ nullptr };

        // What each track's controller last sent, and the newest move waiting for its turn. Kept
        // apart from the controls, so building the strips again doesn't lose a waiting move.
        struct MixerSend
        {
            std::chrono::steady_clock::time_point SentAt{};
            std::optional<uint32_t> Waiting{};
        };

        std::map<std::pair<std::wstring, uint8_t>, MixerSend> m_mixerSends{};
        xaml::DispatcherTimer m_mixerSendTimer{ nullptr };
        std::chrono::steady_clock::time_point m_mixerSendDue{};

        // The generator editor's drawings, redrawn as the clip plays, and where the clip is.
        canvasXaml::CanvasControl m_ringCanvas{ nullptr };
        controls::TextBlock m_passCaption{ nullptr };
        controls::TextBlock m_generatorFooter{ nullptr };
        std::vector<canvasXaml::CanvasControl> m_generatorCanvases{};
        std::vector<controls::Grid> m_ringRows{};
        int64_t m_editorPlayhead{ -1 };
        uint64_t m_editorPass{ 0 };
        size_t m_selectedRing{ SIZE_MAX };

        // A Euclidean rhythm's roll, and its title, which go beside the rings table when there's
        // room and under it when there isn't.
        xaml::FrameworkElement m_ringsRollTitle{ nullptr };
        xaml::FrameworkElement m_ringsRoll{ nullptr };

        // A ring being turned: the rings as they'd be, kept as one edit when it's let go.
        struct RingDrag
        {
            bool Active{ false };
            bool Moved{ false };
            size_t Ring{ SIZE_MAX };
            double StartTurn{ 0 };
            uint8_t StartRotate{ 0 };
            std::vector<seq::EuclidRing> Rings{};
        };

        RingDrag m_ringDrag{};

        // Design section 28's panels. An edit the panel already shows doesn't build it again, so a
        // box keeps its caret while it's typed in.
        bool m_keepGeneratorPanel{ false };

        // The pass a generator's drawing shows while it isn't playing, picked by the person.
        uint64_t m_pickedPass{ 0 };
        std::wstring m_pickedPassClip{};

        std::vector<controls::RadioButton> m_generatorTypeChoices{};

        // The open panel's own sizing, run when the editor changes size.
        std::function<void()> m_fitGenerator{};

        // The room a generator's drawing has beside its settings and the key and range, or
        // beside its settings alone. MakeGeneratorLayout's fit works it out before the panel's own.
        double m_generatorRoomBeside{ 0 };
        double m_generatorRoomAlone{ 0 };

        // Cells being drawn on a Life board: on or off, kept as one edit when it's let go.
        struct LifePaint
        {
            bool Active{ false };
            bool On{ true };
            std::vector<seq::LifeCell> Cells{};
        };

        LifePaint m_lifePaint{};

        // An end of the key and range being dragged along its keyboard, or the whole range.
        struct RangeDrag
        {
            bool Active{ false };
            int32_t Grip{ 0 };              // 0 the lowest note, 1 the highest, 2 both
            int32_t From{ 0 };              // the key it was picked up on
            uint8_t StartLow{ 0 };
            uint8_t StartHigh{ 0 };
            uint8_t Low{ 0 };
            uint8_t High{ 0 };
        };

        RangeDrag m_rangeDrag{};

        // The last wheel notch the range keyboard took, so the same notch can't move it twice.
        uint64_t m_lastRangeWheel{ 0 };
        uint64_t m_lastRingWheel{ 0 };
        bool m_generatorPanelQueued{ false };

        // An edge of a generator's velocity range being dragged, or the whole band.
        struct VelocityDrag
        {
            bool Active{ false };
            int32_t Grip{ 0 };              // 0 the lowest, 1 the highest, 2 both
            uint16_t From{ 0 };             // the velocity it was picked up at
            uint16_t StartLow{ 0 };
            uint16_t StartHigh{ 0 };
            uint16_t Low{ 0 };
            uint16_t High{ 0 };
        };

        VelocityDrag m_velocityDrag{};
        uint64_t m_lastVelocityWheel{ 0 };

        // The velocity range's boxes. A change to the range is shown in place rather than by
        // building the panel again, so these are set to match, and their own change is ignored.
        controls::NumberBox m_velocityLowBox{ nullptr };
        controls::NumberBox m_velocityHighBox{ nullptr };
        bool m_settingVelocityBoxes{ false };

        // The list of clips and tracks to listen to, which gets the focus back after a pick
        // builds the panel again.
        controls::ComboBox m_listenSources{ nullptr };

        // Words typed into a generator, kept a moment after the last key: the text, and the clip it
        // was typed for. Anything that edits the generator, opens another clip or closes the
        // editor commits it first, so nothing typed is lost or lands in another clip. A words box
        // built again takes over from the one it replaces instead, so building the panel never
        // edits the clip in the middle of being built.
        xaml::DispatcherTimer m_generatorTextTimer{ nullptr };
        std::shared_ptr<std::wstring> m_generatorTextPending{};
        std::wstring m_generatorTextClip{};
        void FlushGeneratorText() noexcept;
        std::shared_ptr<std::wstring> WordsStillTyped(_In_ std::wstring const& clipId) const noexcept;
        void TrackTypedWords(_In_ xaml::DispatcherTimer const& timer, _In_ std::shared_ptr<std::wstring> pending, _In_ std::wstring const& clipId, _In_ bool waiting) noexcept;

        // The clip the header's Bars box had the focus for.
        std::wstring m_generatorBarsClip{};

        // Dragging a point on an automation lane or a tempo change, or drawing on a lane.
        struct PointDrag
        {
            enum class Kind : uint8_t { None, LanePoint, Pencil, TempoPoint };

            Kind What{ Kind::None };
            bool Pinned{ false };
            bool Moved{ false };
            std::wstring TrackId{};
            std::wstring LaneId{};
            size_t Point{ SIZE_MAX };
            foundation::Point Start{};
            float RowTop{ 0 };
            float RowHeight{ 0 };

            // The tempo row's scale when the drag began.
            seq::TempoRange Range{};

            // The point's value when the drag began, which Shift keeps while it moves.
            double StartBeatsPerMinute{ 0 };
            uint32_t StartValue{ 0 };

            std::vector<seq::AutomationLane> LanesBefore{};
            std::vector<seq::TempoPoint> TempoBefore{};

            // What the pencil has drawn so far, by tick, and where it last was.
            std::map<int64_t, uint32_t> Drawn{};
            int64_t LastTick{ -1 };
            uint32_t LastValue{ 0 };
        };

        PointDrag m_pointDrag{};

        // The selected automation point or tempo change, which Delete removes.
        std::wstring m_selectedLaneTrackId{};
        std::wstring m_selectedLaneId{};
        size_t m_selectedLanePoint{ SIZE_MAX };
        size_t m_selectedTempoPoint{ SIZE_MAX };

        // Each automation header's value chip, by track and lane.
        struct LaneChip
        {
            std::wstring TrackId{};
            std::wstring LaneId{};
            controls::TextBlock Text{ nullptr };
        };

        std::vector<LaneChip> m_laneChips{};
        winrt::hstring m_textEmptyLane{};
        winrt::hstring m_textClipLane{};
        winrt::hstring m_textEmptyLyrics{};

        // The words over the timeline as they're sung, and the lines they fall into, worked out
        // again only when the sequence changes.
        controls::Grid m_lyricsBar{ nullptr };
        shapes::Rectangle m_lyricsBarPlate{ nullptr };
        controls::TextBlock m_lyricsBarLine{ nullptr };
        controls::TextBlock m_lyricsBarNext{ nullptr };

        // A line with ruby, a syllable at a time with its reading above it.
        controls::StackPanel m_lyricsBarCells{ nullptr };
        bool m_lyricsBarShown{ true };
        std::vector<seq::LyricLine> m_lyricLines{};
        uint64_t m_lyricLinesVersion{ UINT64_MAX };
        void const* m_lyricLinesData{ nullptr };
        size_t m_lyricsBarLineIndex{ SIZE_MAX };
        size_t m_lyricsBarSungCount{ SIZE_MAX };

        seq::Palette m_palette{};

        // The themes the app carries, the one in use, and it worked out for light and dark with
        // the window color. Set by ApplyStyle; the palette is the half the window shows.
        std::vector<std::shared_ptr<themeengine::ThemeDefinition const>> m_styles{};
        std::shared_ptr<themeengine::ThemeDefinition const> m_style{};
        std::shared_ptr<themeengine::ResolvedTheme const> m_styleLight{};
        std::shared_ptr<themeengine::ResolvedTheme const> m_styleDark{};
        themeengine::XamlThemeResources m_styleResources{};

        // The window color the style was last worked out with (0xAARRGGBB), or none.
        std::optional<uint32_t> m_styleTint{};

        seq::ArrangeRenderer m_renderer{};

        // Each canvas drawn while a sequence plays draws through a picture of its own, so its text
        // stays put (ThemeCanvas.h).
        themeengine::SteadyCanvas m_steadyRuler{};
        themeengine::SteadyCanvas m_steadyLanes[2]{};
        themeengine::SteadyCanvas m_steadyLaunchers[2]{};
        themeengine::SteadyCanvas m_steadyEditor{};
        seq::TimelineBar m_timelineBar{};
        themeengine::PlateLayer m_transportPlates{};
        themeengine::PlateLayer m_toolbarPlates{};
        themeengine::PlateLayer m_editorPlates{};

        // Behind the track headers ("rows") and the scene headers ("scenes").
        themeengine::PlateLayer m_headerPlates{};
        double m_defaultTransportHeight{ 52 };

        // The transport's colors: the window's, or the dark look's on a style's dark strip.
        seq::Palette m_stripPalette{};
        double m_defaultTransportSpacing{ 6 };
        seq::PianoRoll m_roll{};
        seq::ClipLaneStrip m_clipLanes{};
        bool m_clipLaneGesture{ false };
        seq::PatternEditor m_pattern{};
        bool m_updatingPatternUi{ false };
        bool m_patternGesture{ false };

        // The key a melodic template is put in, 0 (C) to 11 (B), until the window closes.
        uint8_t m_templateKey{ 0 };
        std::optional<uint8_t> m_auditioning{};
        std::chrono::steady_clock::time_point m_lastEditorPressTime{};
        foundation::Point m_lastEditorPressPoint{};
        bool m_editorGesture{ false };
        bool m_editorSizing{ false };

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

        // A refresh asked for while one runs: that one runs again when it's done, since the
        // change may have come after it looked.
        std::atomic<bool> m_endpointRefreshAgain{ false };
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

        // Notes echoed and not let go yet, under m_inputLock: by track, then by group, channel and
        // note, each with the note-off that ends it. When a track stops echoing a key that's held,
        // or echoes it somewhere else, UpdateEchoRoutes sends the note-off it would have had.
        struct EchoedNotes
        {
            std::wstring SourceId{};
            seq::TrackDestination Destination{};
            std::map<uint32_t, std::array<uint32_t, 2>> Offs{};
        };

        std::map<std::wstring, EchoedNotes> m_echoedNotes{};

        // Tracks with an arpeggiator that plays the keys held on their source, read on the
        // service's thread under m_inputLock. The UI thread keeps the ids, to tell the engine to
        // forget the keys of a track that stops listening.
        struct ArpeggiatorRoute
        {
            std::wstring TrackId{};
            std::wstring SourceId{};
            seq::RecordFilter Filter{};
        };

        std::vector<ArpeggiatorRoute> m_arpeggiatorRoutes{};
        std::unordered_set<std::wstring> m_arpeggiatorTracks{};
        std::map<std::wstring, seq::RecordingTake> m_takes{};

        // A take being recorded into a launcher slot. m_slotTake belongs to the UI thread; the take
        // itself is fed on the service's thread, under m_inputLock. Places are unwrapped ticks.
        struct SlotTake
        {
            std::wstring TrackId{};
            size_t Scene{ 0 };
            std::wstring ClipId{};
            int64_t Start{ 0 };
            int64_t Length{ 0 };
            int64_t BarTicks{ 3840 };
            int64_t LastSeen{ 0 };
            std::chrono::steady_clock::time_point StartedAt{};
            bool LaunchQueued{ false };
            bool Published{ false };
        };

        struct SlotInput
        {
            std::wstring TrackId{};
            int64_t Start{ 0 };
            int64_t Length{ 0 };
            seq::RecordingTake Take{ 0 };
        };

        std::optional<SlotTake> m_slotTake{};
        std::optional<SlotInput> m_slotInput{};
        std::atomic<bool> m_slotRecording{ false };

        // The last few minutes each armed track heard, for Capture.
        struct HeardMessage
        {
            uint64_t Timestamp{ 0 };
            std::array<uint32_t, 4> Words{};
            uint8_t WordCount{ 0 };
        };

        std::map<std::wstring, std::deque<HeardMessage>> m_heard{};

        // The device whose clock the sequence follows, read on the service's thread under
        // m_inputLock. Its Start and Stop reach the window through the queue.
        std::wstring m_followSourceId{};
        uint8_t m_followGroup{ 0 };
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_uiQueue{ nullptr };
        std::chrono::steady_clock::time_point m_clockStatusAt{};

        // Remote control: the sequencer's own endpoint, and what a surface was last told.
        std::shared_ptr<seq::RemoteEndpoint> m_remote{};

        // The endpoint turned off last, still closing in the background. Turned on again, the new
        // one waits for it: the service won't make a second endpoint with the same id.
        winrt::Windows::Foundation::IAsyncAction m_remoteClosing{ nullptr };
        bool m_remoteStarting{ false };
        std::optional<seq::RemoteState> m_remoteShown{};
        xaml::DispatcherTimer m_remoteTimer{ nullptr };
        uint32_t m_remoteTicks{ 0 };

        // MIDI-CI: a connection and presence for each device asked for its sounds, kept until
        // the window closes so the device sees one identifier. The lists it shared are kept by
        // device, group and channel, on the UI thread.
        struct CiLink
        {
            midi2::MidiEndpointConnection Connection{ nullptr };
            std::unique_ptr<midiapp::MidiCiPresence> Presence{};
        };

        std::mutex m_ciLock{};
        std::map<std::wstring, CiLink> m_ciLinks{};
        std::shared_ptr<midiapp::MidiCiProgramListQuery> m_soundQuery{};
        std::map<std::wstring, std::vector<midiapp::ProgramListEntry>> m_deviceSounds{};
        std::weak_ptr<SoundDialogState> m_soundDialog{};

        xaml::DispatcherTimer m_frameTimer{ nullptr };
        xaml::DispatcherTimer m_serviceTimer{ nullptr };
        xaml::DispatcherTimer m_autosaveTimer{ nullptr };
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
