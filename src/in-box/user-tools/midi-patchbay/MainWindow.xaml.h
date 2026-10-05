// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "EndpointCatalog.h"
#include "PatchCanvas.h"
#include "PatchGraph.h"
#include "PatchLibrary.h"
#include "PatchModel.h"
#include "PatchStore.h"
#include "RouteEngine.h"
#include "ThemeBrushes.h"

namespace winrt::midipatchbay::implementation
{
    // One patch, open for editing. Each patch gets a window of its own, opened from the library.
    // The library owns the patches, the routing and the notification area; this window shows
    // one patch and changes it.
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        // Called before Activate. False when there is no such patch.
        bool OpenPatch(_In_ std::wstring const& patchKey) noexcept;

        std::wstring const& PatchKey() const noexcept { return m_patchKey; }

        // Each editor is nudged down and across from the last, so a second one is seen to open.
        void RestoreWindowPlacement(_In_ int32_t cascade) noexcept;

        // The theme or the backdrop changed in the library.
        void ApplyAppearance() noexcept;

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnSavePatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnPatchMenuClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRoutingToggleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAutoStartToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnAutoStartBarCloseClick(_In_ controls::InfoBar const& sender, _In_ foundation::IInspectable const& args);
        void OnNotRoutingStartClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnShowEarlierVersionClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnUndoClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRedoClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCutClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCopyClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPasteClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnDeleteClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnAddEndpointClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCreateLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAutoArrangeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomFitClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomInClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomApplyClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomFlyoutOpening(foundation::IInspectable const& sender, foundation::IInspectable const& args);
        void OnZoomValueChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnTestClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnShowLoopClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnInspectorCloseClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnLibraryButtonClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnConversionBarCloseClick(_In_ controls::InfoBar const& sender, _In_ foundation::IInspectable const& args);

        void OnSavePatchNameChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnLoopbackNameChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);

        void OnPaletteTabChanged(
            _In_ controls::SelectorBar const& sender,
            _In_ controls::SelectorBarSelectionChangedEventArgs const& args);
        void OnPaletteSearchChanged(_In_ foundation::IInspectable const& sender, _In_ controls::TextChangedEventArgs const& args);

        // Which of the transform dialog's mapping tables a row belongs to. Public only so the
        // helpers in MainWindowTransforms.cpp can name it; nothing is projected from here.
        enum class TransformMap : int32_t
        {
            Note = 0,
            Control = 1,
            Channel = 2,
            Program = 3,
            BankMsb = 4,
            BankLsb = 5,
        };

        static constexpr size_t TransformMapCount = 6;

        // Which end of which range a value shape box edits. Public for the same reason.
        enum class ShapeField : int32_t
        {
            InputMinimum = 0,
            InputMaximum = 1,
            OutputMinimum = 2,
            OutputMaximum = 3,
        };

        // A shape editor names its shape by this, not by pointer, because the controller rows
        // are rebuilt whenever one is added or removed.
        static constexpr int32_t AftertouchShapeIndex = -1;

    private:
        // ---- startup and chrome, in MainWindow.xaml.cpp ----
        void InitializeWindowChrome() noexcept;
        void InitializeStaticText() noexcept;
        void UpdateTitle() noexcept;
        void SaveEditorPlacement() noexcept;
        void HandleKeyDown(_In_ input::KeyRoutedEventArgs const& args) noexcept;

        // Null once the patch is gone. Looked up every time, so nothing here holds a pointer
        // across a suspend or a library change.
        ::midipatchbay::PatchDocument* CurrentPatch() noexcept;
        ::midipatchbay::PatchAnalysis const& Analysis() noexcept;

        void OnLibraryChanged(_In_ ::midipatchbay::LibraryChange change, _In_ std::wstring const& key) noexcept;

        // Every change to the patch ends here: it becomes a step that Undo can take back, the
        // library saves it and rebuilds the routes, and the window shows the result. A change the
        // canvas already shows, such as a node moved, needs no redraw.
        void CommitChange(_In_ bool routingAffected = true, _In_ bool redraw = true) noexcept;

        // ---- undo, in MainWindow.xaml.cpp ----
        // Each state is the patch file text of the parts the canvas edits. That is far smaller
        // than a copy of every step's settings, and comparing two is a string compare.
        std::wstring CaptureState() noexcept;
        void RestoreState(_In_ std::wstring const& state) noexcept;
        void Undo() noexcept;
        void Redo() noexcept;
        void UpdateCommandStates() noexcept;

        // ---- clipboard, in MainWindow.xaml.cpp ----
        // The selected nodes and the links between them, as patch file text, so a copy can be
        // pasted into another patch, or read by a person.
        std::wstring SelectionAsText() noexcept;
        void CopySelection() noexcept;
        void CutSelection() noexcept;
        void DuplicateSelection() noexcept;
        winrt::fire_and_forget PasteAsync();

        // Endpoints already on this patch are reused rather than added twice. False when nothing
        // in the text could be pasted.
        bool PasteText(_In_ std::wstring const& text) noexcept;

        // ---- canvas, in MainWindow.xaml.cpp ----
        void RebuildCanvas() noexcept;
        void OnCanvasSelectionChanged() noexcept;
        void OnConnectionRequested(_In_ ::midipatchbay::PatchConnection connection) noexcept;
        void OnConnectionRetargetRequested(
            _In_ std::wstring const& connectionId,
            _In_ ::midipatchbay::PatchConnection updated) noexcept;
        void OnCanvasLayoutChanged() noexcept;

        // True when the link can be added. Says why when it can't. The link being moved, if any,
        // is left out of the checks.
        bool CanConnect(
            _In_ ::midipatchbay::PatchConnection const& candidate,
            _In_ std::wstring const& ignoreConnectionId) noexcept;

        // The part of the canvas in view, in canvas units.
        foundation::Rect VisibleCanvasRect() noexcept;

        void ApplyZoom(_In_ float zoom) noexcept;
        void UpdateZoomText(_In_ float zoom) noexcept;

        // ---- palette and steps, in MainWindow.xaml.cpp ----
        void RebuildPalette() noexcept;
        xaml::UIElement BuildBlockTile(_In_ ::midipatchbay::BlockKind kind) noexcept;
        xaml::UIElement BuildEndpointTile(
            _In_ std::wstring const& endpointDeviceId,
            _In_ std::wstring const& name,
            _In_ std::wstring const& detail,
            _In_ bool onCanvas) noexcept;

        // The point is where the middle of the step goes. Returns the new step's id, or empty.
        std::wstring AddBlock(_In_ ::midipatchbay::BlockKind kind, _In_ foundation::Point const& center) noexcept;

        // Splits a link in two with a new step in the middle. The first half keeps the link's id
        // and its mute, so its counts carry on.
        void InsertBlockIntoConnection(
            _In_ ::midipatchbay::BlockKind kind,
            _In_ std::wstring const& connectionId,
            _In_ std::optional<foundation::Point> const& center) noexcept;

        void OnBlockDropped(
            _In_ ::midipatchbay::BlockKind kind,
            _In_ foundation::Point const& point,
            _In_ std::wstring const& connectionId) noexcept;
        void OnEndpointDropped(_In_ std::wstring const& endpointDeviceId, _In_ foundation::Point const& point) noexcept;

        void SetBlockBypassed(_In_ std::wstring const& blockId, _In_ bool bypassed) noexcept;

        // ---- step settings, in MainWindowFilters.cpp ----
        winrt::fire_and_forget ShowBlockDialogAsync(std::wstring blockId);
        void BuildBlockDialog() noexcept;
        void UpdateBlockSummary() noexcept;

        // What the dialog holds right now, in the shape a step stores.
        ::midipatchbay::BlockSettings EditedSettings() noexcept;

        void BuildMessageTypeSections() noexcept;
        void BuildChannelSection() noexcept;
        void BuildGroupFilterSection() noexcept;
        void BuildValueSetSection() noexcept;
        void RefreshValueSetUi() noexcept;
        void OnValueKeyPressed(_In_ uint8_t value, _In_ bool shift) noexcept;
        void BuildVelocityFilterSection() noexcept;
        void BuildMaskSection() noexcept;
        void RebuildMaskConditions() noexcept;

        // Fills the value set from what a device plays, for as long as the dialog is open.
        winrt::fire_and_forget StartLearningAsync();
        void StopLearning() noexcept;
        void OnLearnedValue(_In_ uint8_t value) noexcept;
        std::vector<std::wstring> SourcesFeeding(_In_ std::wstring const& blockId) noexcept;

        // ---- step settings that change messages, in MainWindowTransforms.cpp ----
        void PrepareTransformRows() noexcept;
        void BuildTransformSections() noexcept;
        void BuildValueScaleSection() noexcept;
        void BuildTransposeSection() noexcept;
        void BuildVelocitySection() noexcept;
        void BuildGroupMapSection() noexcept;
        void BuildThrottleSection() noexcept;

        // ---- generators and the clock divider, in MainWindowGenerators.cpp ----
        void BuildClockGeneratorSections() noexcept;
        void BuildTimeCodeGeneratorSections() noexcept;
        void BuildLfoGeneratorSections() noexcept;
        void BuildClockDividerSection() noexcept;
        controls::Border BuildGeneratorGroupCard(_Inout_ uint8_t* group) noexcept;
        void ApplyStartTimeText() noexcept;
        void RefreshLfoMessageUi() noexcept;
        void RefreshGeneratorCaptions() noexcept;

        // The mapping tables are edited the same way, so one set of row functions drives them all.
        controls::StackPanel BuildMapSection(_In_ TransformMap which) noexcept;
        void RebuildMapRows(_In_ TransformMap which) noexcept;
        void RefreshMapRowLabels(_In_ TransformMap which) noexcept;
        void ApplyValueScale() noexcept;
        void RefreshVelocityEnabledState() noexcept;
        void CommitTransformMaps() noexcept;
        void DrawVelocityCurve() noexcept;
        winrt::fire_and_forget PlayTestNoteAsync(_In_ uint8_t note);

        // Controller values and aftertouch share one editor, found through EditingShape.
        xaml::UIElement BuildAftertouchSection() noexcept;
        controls::StackPanel BuildControlValueSection() noexcept;
        void RebuildControlValueRows() noexcept;
        void DrawShapePreviews() noexcept;
        ::midipatchbay::ValueShape* EditingShape(_In_ int32_t which) noexcept;
        controls::ComboBox ShapeCurveBox(_In_ int32_t which) noexcept;
        controls::CheckBox ShapeInvertBox(_In_ int32_t which) noexcept;
        controls::NumberBox ShapeRangeBox(_In_ int32_t which, _In_ ShapeField field) noexcept;

        // ---- inspector, in MainWindowInspector.cpp ----
        void RefreshInspector() noexcept;
        void BuildEndpointInspector(_In_ ::midipatchbay::PatchEndpoint const& endpoint) noexcept;
        void BuildConnectionInspector(_In_ ::midipatchbay::PatchConnection const& connection) noexcept;
        void BuildBlockInspector(_In_ ::midipatchbay::PatchBlock const& block) noexcept;
        void BuildSelectionInspector(_In_ size_t count) noexcept;

        // The steps a customer can put into a link, by category.
        controls::MenuFlyout BuildAddStepMenu(_In_ std::wstring const& connectionId) noexcept;

        // The activity line is the only part of the inspector that changes on its own, so it is
        // updated in place rather than rebuilding the panel on every tick.
        void UpdateInspectorActivity() noexcept;
        winrt::hstring ActivityText(_In_ std::wstring const& elementId) noexcept;

        // "Iridium, group 3" or a step's name, for either end of a link.
        winrt::hstring DescribeLinkEnd(_In_ std::wstring const& nodeId, _In_ int32_t groupIndex, _In_ bool isSource) noexcept;

        // ---- dialogs and menus, in MainWindowDialogs.cpp ----
        // True ticks the dialog's startup box, for the Start automatically switch on a temporary patch.
        winrt::fire_and_forget ShowSavePatchDialogAsync(_In_ bool startAutomatically = false);
        winrt::fire_and_forget ShowCreateLoopbackDialogAsync();
        winrt::fire_and_forget ShowDeletePatchDialogAsync();

        void ShowEndpointPalette(_In_ xaml::FrameworkElement const& anchor) noexcept;
        void ShowTestMenu(_In_ xaml::FrameworkElement const& anchor) noexcept;
        void ShowNodeMenu(_In_ std::wstring const& nodeId, _In_ foundation::Point const& position) noexcept;

        // Without a point, the node goes in the middle of what is in view.
        void AddEndpointToPatch(
            _In_ ::midipatchbay::LiveEndpoint const& endpoint,
            _In_ std::optional<foundation::Point> const& center = std::nullopt) noexcept;
        void AddRememberedEndpointToPatch(
            _In_ ::midipatchbay::PatchEndpoint const& remembered,
            _In_ std::optional<foundation::Point> const& center = std::nullopt) noexcept;

        // Endpoints the customer has used in a saved patch but that are not here now. Derived
        // from the patches themselves rather than a separate list, so nothing extra is stored.
        std::vector<::midipatchbay::PatchEndpoint> RememberedEndpoints() noexcept;

        // Whether an endpoint is on this patch already, by endpoint device id.
        bool IsOnCanvas(_In_ std::wstring const& endpointDeviceId) noexcept;
        bool IsOnCanvas(_In_ ::midipatchbay::PatchEndpoint const& endpoint) noexcept;

        void DeleteSelection() noexcept;
        winrt::fire_and_forget DeleteSelectionAsync();

        void LaunchTool(_In_ std::wstring const& toolExeName, _In_ std::wstring const& endpointDeviceId) noexcept;

        // ---- header and messages, in MainWindowRouting.cpp ----
        void UpdatePatchHeader() noexcept;
        void UpdateMessages() noexcept;
        void UpdateConversionNotice() noexcept;
        void UpdateStatusStrip() noexcept;
        void OnActivity() noexcept;
        void ShowStatus(_In_ winrt::hstring const& message, _In_ controls::InfoBarSeverity severity) noexcept;

        // ---- state ----
        midiapp::WindowChrome m_chrome{};
        ::midipatchbay::PatchCanvas m_canvas{};

        // The patch's SessionKey in the library. Cleared when the patch goes away.
        std::wstring m_patchKey{};
        uint32_t m_libraryToken{ 0 };

        // Set while this window tells the library about its own change, so the change it hears
        // back is not mistaken for somebody else's.
        bool m_committing{ false };

        std::vector<std::wstring> m_undoStates{};
        std::vector<std::wstring> m_redoStates{};
        std::wstring m_committedState{};

        // Pasting the same thing again lands a step further along each time.
        std::wstring m_lastPastedText{};
        int32_t m_pasteRepeat{ 0 };

        // Keyed by link or step id.
        std::unordered_map<std::wstring, ::midipatchbay::RouteStats> m_activity{};

        controls::TextBlock m_activityText{ nullptr };
        std::wstring m_activityElementId{};

        bool m_settingZoomBox{ false };

        // ---- the step dialog works on a copy, so Cancel leaves a running patch alone ----
        std::wstring m_editingBlockId{};
        ::midipatchbay::BlockKind m_editingKind{ ::midipatchbay::BlockKind::MessageTypeFilter };
        ::midipatchbay::BlockSettings m_editingSettings{};
        ::midipatchbay::MessageFilter m_editingFilter{};
        ::midipatchbay::MessageTransform m_editingTransform{};

        bool m_updatingValueSet{ false };
        controls::RadioButtons m_valueModeButtons{ nullptr };
        controls::NumberBox m_valueLowBox{ nullptr };
        controls::NumberBox m_valueHighBox{ nullptr };
        controls::NumberBox m_valueOneBox{ nullptr };
        controls::StackPanel m_valueRangePanel{ nullptr };
        controls::StackPanel m_valueOnePanel{ nullptr };
        controls::TextBlock m_valueSetText{ nullptr };
        primitives::ToggleButton m_learnButton{ nullptr };
        controls::TextBlock m_learnStatusText{ nullptr };

        // The keyboard for notes, a grid of numbers for controllers. One fill per value.
        std::vector<std::pair<uint8_t, shapes::Rectangle>> m_valueKeyFills{};
        std::vector<std::pair<uint8_t, primitives::ToggleButton>> m_valueToggles{};

        // What a range click sets next: the bottom first, then the top.
        bool m_learnedLow{ false };

        // Owned here and closed off the UI thread, because closing waits on the service.
        midi2::MidiSession m_learnSession{ nullptr };
        uint64_t m_learnGeneration{ 0 };

        controls::StackPanel m_maskConditionsPanel{ nullptr };

        // The transform sections edit row lists and collect them back into the sparse arrays.
        std::array<std::vector<std::pair<int32_t, int32_t>>, TransformMapCount> m_mapRows{};

        std::array<controls::StackPanel, TransformMapCount> m_mapPanels{
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

        std::array<std::vector<controls::TextBlock>, TransformMapCount> m_mapLabels{};

        // Every box that shows a fraction of full scale, so switching between 0 to 127 and
        // percent can retext them all in place.
        controls::NumberBox m_fixedVelocityBox{ nullptr };
        controls::NumberBox m_minimumVelocityBox{ nullptr };
        controls::NumberBox m_maximumVelocityBox{ nullptr };
        controls::CheckBox m_velocityRescaleCheck{ nullptr };
        controls::TextBlock m_transposeExampleText{ nullptr };
        controls::Canvas m_velocityCurveCanvas{ nullptr };
        bool m_applyingValueScale{ false };

        // One row per controller whose value is reshaped, collected back into the dense array
        // on Apply the same way the mapping tables are.
        struct ControlValueRow
        {
            int32_t Controller{ 1 };
            ::midipatchbay::ValueShape Shape{};
        };

        std::vector<ControlValueRow> m_controlValueRows{};
        controls::StackPanel m_controlValuePanel{ nullptr };

        // Parallel to m_controlValueRows.
        std::vector<controls::Canvas> m_controlValuePreviews{};

        controls::Canvas m_aftertouchPreview{ nullptr };

        // Every range box in a shape editor, so switching between 0 to 127 and percent can
        // retext them the way it does the velocity boxes.
        struct ShapeRangeBoxEntry
        {
            controls::NumberBox Box{ nullptr };
            int32_t Which{ AftertouchShapeIndex };
            ShapeField Field{ ShapeField::InputMinimum };
        };

        std::vector<ShapeRangeBoxEntry> m_shapeRangeBoxes{};

        // The generator sections' parts that follow another setting.
        controls::TextBlock m_swingCaption{ nullptr };
        controls::TextBox m_startTimeBox{ nullptr };
        controls::TextBlock m_startTimeCaption{ nullptr };
        controls::TextBlock m_lfoRateCaption{ nullptr };
        controls::TextBlock m_lfoIntervalCaption{ nullptr };
        controls::NumberBox m_lfoNumberBox{ nullptr };
        controls::NumberBox m_lfoBankBox{ nullptr };
        controls::NumberBox m_lfoIndexBox{ nullptr };
        controls::RadioButtons m_lfoProtocolButtons{ nullptr };
        controls::TextBlock m_dividerCaption{ nullptr };

        // Where the audition button plays, captured when the dialog opens.
        std::wstring m_testEndpointDeviceId{};
        int32_t m_testGroupIndex{ ::midipatchbay::AllGroups };

        uint64_t m_lastTotalMessages{ 0 };
        std::chrono::steady_clock::time_point m_lastRateSample{};

        bool m_loaded{ false };
        bool m_closing{ false };

        winrt::event_token m_closedToken{};
    };
}

namespace winrt::midipatchbay::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
