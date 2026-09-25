// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "EditorWindow.g.h"

#include "EditorItems.h"
#include "WindowChrome.h"
#include "LayoutModel.h"
#include "ThemeModel.h"
#include "EditorController.h"
#include "LivePlayer.h"
#include "SurfaceRenderer.h"
#include "DeckBrush.h"
#include "InputRouter.h"

namespace winrt::midiglass::implementation
{
    // One layout being edited. Only one editor window exists at a time: editing the same layout
    // in two windows is a conflict nobody needs, and a second request focuses the first.
    //
    // Everything that changes the document goes through glass::EditorController, which knows
    // nothing about XAML. This file is the view: it draws what the controller holds and turns
    // pointers and keys into calls on it.
    struct EditorWindow : EditorWindowT<EditorWindow>
    {
        EditorWindow() = default;

        // Not projected. Called before Activate, so the layout is read before the first paint.
        bool LoadLayout(_In_ std::wstring const& filePath);

        std::wstring const& LayoutFilePath() const noexcept { return m_filePath; }

        void OnRootLoaded(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        // ---- toolbar ----

        void OnAppearanceButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnLibraryClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnUndoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRedoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSnapToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnGridSizeChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnArrangeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRepeatClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnPageSizeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnBackgroundClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRenameLayoutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // Where this designer was when it was last closed, nudged along by the cascade offset
        // so a second one does not land exactly on the first. Called before Activate.
        void RestoreWindowPlacement(_In_ int32_t cascadeOffset);

        void OnLayoutSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCloseLayoutSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSettingsAddPageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSettingsAddDeviceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSettingsLookAgainClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnShowAllGroupsChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- setting the keyboard order by clicking (EditorKeyboardOrder.cpp) ----

        void OnKeyboardOrderDoneClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnKeyboardOrderRestartClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnKeyboardOrderCancelClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSortKeyboardOrderClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRunClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- Edit and Try (EditorTryMode.cpp) ----

        void OnEditModeChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnEditModeUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTryModeChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTryModeUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnMonitorSelectedOnlyToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorPauseToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorClearClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMonitorExpandClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- learn (EditorLearn.cpp) ----

        void OnLearnOneToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnLearnBankToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- accelerators ----

        void OnUndoAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnRedoAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnSaveAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnDuplicateAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnSelectAllAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnDeleteAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnEscapeAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnTryAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        void OnZOrderAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        // ---- palette and outline ----

        void OnPaletteSearchChanged(controls::AutoSuggestBox const& sender, controls::AutoSuggestBoxTextChangedEventArgs const& args);
        void OnAddToPageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnOutlineSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnOutlineMoveUp(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnOutlineMoveDown(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- canvas ----

        void OnCanvasSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);
        void OnCanvasPointerPressed(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);
        void OnCanvasPointerMoved(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);
        void OnCanvasPointerReleased(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);
        void OnCanvasPointerCaptureLost(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);
        void OnSelectOffPageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAddPageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomInClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomFitClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- panes (EditorPanes.cpp) ----

        void OnSplitterPressed(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);
        void OnSplitterMoved(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);
        void OnSplitterReleased(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);
        void OnSplitterEntered(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);        void OnSplitterExited(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args);
        void OnCollapseLeftPaneClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnExpandLeftPaneClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- inspector ----

        void OnBoundsChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnAspectLockToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnLabelChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnKindChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnHueSlotClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnStyleChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnStyleUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnLabelPlacedChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnLabelWidthChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnLabelFontClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnShowValueChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

        void OnDuplicateSelectionClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnDeleteSelectionClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnInspectorTabChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnInspectorTabUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnLeftTabChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnLeftTabUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnMessageSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnAddMessageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRemoveMessageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMessageFieldChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnMessageNumberChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);

        void OnSysExChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSysExFromFileClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRawWordsChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSequenceChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnNewSequenceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnEditSequenceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTargetPageChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

        void OnSendOnStartToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnReturnsToDefaultToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnDefaultValueChanged(foundation::IInspectable const& sender, controls::Primitives::RangeBaseValueChangedEventArgs const& args);
        void OnSendIntervalChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnPickupChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnKeyboardOrderChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);

        // ---- per-kind properties (EditorControlProperties.cpp) ----

        void OnTicksShowChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTickCountChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnShowDetentValuesChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnChoosePictureClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRemovePictureClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnPictureFitChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnPictureOpacityChanged(foundation::IInspectable const& sender, controls::Primitives::RangeBaseValueChangedEventArgs const& args);
        void OnPictureLoopsChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnKeyCountChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnLowestNoteChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnKeyColorChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnKeyVelocityChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnSpringTargetChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnDragAxisChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

        void OnClockBpmChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnClockRangeChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnClockSourceChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnClockFlagChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- what a control listens for (EditorControlProperties.cpp) ----

        void OnFeedbackEnabledToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnFeedbackModeChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnFeedbackDeviceChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnFeedbackGroupChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnFeedbackChannelChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnFeedbackKindChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnFeedbackTempoChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnFeedbackMatchChannelChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnFeedbackNumberChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnFeedbackHoldChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);

        void OnDetentModeChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnDetentStepChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnDetentStopsChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        void OnWindowClosed(foundation::IInspectable const& sender, xaml::WindowEventArgs const& args);

        // ---- canvas (EditorCanvas.cpp) ----

        void BuildPage();
        void RebuildSurface();

        // Bracket and square bracket have no name in the virtual key enum, so they cannot be
        // written in markup and are attached here instead.
        void AddZOrderAccelerators();
        void RebuildGrid();
        void UpdateOverlay();

        // Follows a move drag without rebuilding the page. A rebuild costs about five
        // milliseconds at two hundred controls and a drag has one frame to spend.
        void MoveDraggedItems();
        void ResizeDraggedItems();

        void ApplyCanvasScale();
        void StepZoom(_In_ double factor);

        // True when the work area moved or resized, which means the grid and the overlay are
        // drawn against the wrong origin until they are redrawn.
        bool UpdateWorkArea();
        void UpdateDeckBrushes();
        void UpdateOffPageBar();
        void RebuildPageRail();

        // A page tab or a sequence step asked for another page. One path, so a page change
        // cannot half happen.
        void ShowEditorPage(_In_ size_t pageIndex);

        void InitializeSplitters();

        // The pane widths, the monitor height and the zoom, so the designer opens where it was
        // left. The window's own position and size go with them.
        void RestoreEditorPanes();
        void SaveEditorPlacement();

        // Work area coordinates to page coordinates. The work area starts at a negative offset,
        // so this is not a no-op even at a scale of one.
        glass::EditRect WorkArea() const noexcept { return m_workArea; }
        double PointToPageX(_In_ double x) const noexcept { return x + m_workArea.X; }
        double PointToPageY(_In_ double y) const noexcept { return y + m_workArea.Y; }

        std::wstring HitTest(_In_ double pageX, _In_ double pageY) const;
        glass::ResizeHandle HitTestHandle(_In_ double pageX, _In_ double pageY) const;

        // The selected control's label gets its own outline and its own four corners, so a
        // caption can be given room a narrow control does not have.
        //
        // A label under a control shares an edge with it, so both hit tests report how far away
        // the handle was and the caller takes the nearer set.
        glass::ResizeHandle HitTestHandle(_In_ double pageX, _In_ double pageY, _Out_ double& distance) const;
        glass::ResizeHandle HitTestLabelHandle(_In_ double pageX, _In_ double pageY, _Out_ double& distance) const;

        bool TryGetLabelRect(_In_ glass::Control const& control, _Out_ glass::EditRect& rect) const;
        bool HitTestLabelBody(_In_ double pageX, _In_ double pageY) const;

        bool BeginLabelDrag(_In_ glass::ResizeHandle handle);
        void UpdateLabelDrag(_In_ double deltaX, _In_ double deltaY);

        void SetKeyboardOrderMode(_In_ bool active);
        void UpdateKeyboardOrderBar();
        bool KeyboardOrderNumberFor(_In_ std::wstring const& id, _Out_ int32_t& number) const;
        bool HandleKeyboardOrderPress(_In_ double pageX, _In_ double pageY);

        // ---- palette and outline (EditorOutline.cpp) ----

        void BuildPalette();
        void OnPaletteTileClick(_In_ controls::Primitives::ToggleButton const& sender, _In_ glass::ControlKind kind);
        void OnPaletteTileDoubleTapped(_In_ glass::ControlKind kind);

        // Press a tile, drag onto the page, let go. The control appears as soon as the pointer
        // reaches the canvas and follows it from there.
        void OnPaletteTilePressed(
            _In_ controls::Primitives::ToggleButton const& sender,
            _In_ glass::ControlKind kind,
            _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnPaletteTileMoved(_In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnPaletteTileReleased(
            _In_ controls::Primitives::ToggleButton const& sender,
            _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void SyncPaletteSelection();
        void RebuildOutline();
        void SelectOutlineRowForSelection();

        // ---- inspector (EditorInspector.cpp) ----

        void BuildInspectorChoices();
        void RefreshInspector();
        void RefreshInspectorGeometry();
        void RefreshMessageList();
        void RefreshMessageFields();

        // Which of the message rows make sense for the kind of message this is.
        void RefreshMessageKindFields(_In_ glass::Control const& control);
        void RefreshMessageKindFields(_In_ glass::ControlMessage const& message);
        void RefreshSequenceChoices(_In_ std::wstring const& selectedName);

        // The groups the chosen device actually declares, so a control cannot be pointed at one
        // that goes nowhere. m_groupChoices maps a combo index back to a group number.
        void RefreshGroupChoices(_In_ std::wstring const& deviceName, _In_ int32_t selectedGroup);

        // Reads the selected message, hands it to the caller to change, and writes it back if
        // the caller says something changed. One path, so every payload field is saved the same
        // way and none of them can forget to mark the layout dirty.
        bool TryEditSelectedMessage(_In_ std::function<bool(glass::ControlMessage&)> const& edit);
        void SyncStyleSegments(_In_ glass::ControlStyleOverride style);
        void RefreshPreview(_In_ glass::Control const& control);

        void SelectInspectorTab(_In_ int32_t index);
        void SelectLeftTab(_In_ int32_t index);

        // ---- per-kind properties (EditorControlProperties.cpp, EditorControlEdits.cpp) ----

        void BuildControlPropertyChoices();

        // The panels only some kinds of control have. A page of settings that do nothing for
        // the control in front of you is worse than a shorter page.
        void RefreshKindPanels(_In_ glass::Control const& control);
        void RefreshFeedbackPanel(_In_ glass::Control const& control);

        // The knobs and faders a clock can take its tempo from, and the clocks a lamp can
        // follow. Both map a combo index back to a control id.
        void RefreshTempoSourceChoices(_In_ glass::Control const& control);
        void RefreshBeatSourceChoices(_In_ glass::Control const& control);

        void RefreshFeedbackDeviceChoices(_In_ std::wstring const& selectedName);
        void RefreshFeedbackGroupChoices(_In_ int32_t selectedGroup);

        // One path for every per-kind edit, so none of them can forget to redraw the surface or
        // mark the layout changed.
        void ApplyControlEdit(
            _In_ std::wstring const& id,
            _In_ std::function<bool(std::wstring const&)> const& edit);

        void ApplyKeyboardEdit();
        void ApplyClockEdit();
        void ApplyFeedbackEdit();

        // The full path of a picture or video the customer chose, or empty. Does not copy it.
        std::wstring PickControlPictureFile();

        // What the armed palette tool is called, for the status bar.
        std::wstring NameForArmedKind() const;

        glass::Control const* SingleSelectedControl() const;

        // ---- saving ----

        void MarkChanged();
        void SaveNow();
        void UpdateSavedChip();
        void UpdateStatusBar();

        // ---- dialogs (EditorDialogs.cpp) ----

        winrt::fire_and_forget ShowRepeatDialog();
        winrt::fire_and_forget ShowPageSizeDialog();

        // The picture behind the whole surface, and how it fits. Choosing one that lives
        // somewhere else says so first, because it gets copied beside the layout.
        winrt::fire_and_forget ShowBackgroundDialog();

        // The full path of a picture the customer chose, or empty. Does not copy anything.
        std::wstring PickBackgroundImageFile();
        winrt::fire_and_forget ShowRenameDialog();

        // ---- the sequence editor (EditorSequenceDialog.cpp) ----

        winrt::fire_and_forget ShowSequenceDialog(_In_ std::wstring sequenceName);

        // ---- the label font dialog (EditorFontDialog.cpp) ----

        winrt::fire_and_forget ShowLabelFontDialog(_In_ std::wstring controlId);

        // ---- layout settings: pages and the device table (EditorLayoutSettings.cpp) ----

        void ShowLayoutSettings(_In_ bool show);
        void RefreshLayoutSettings();
        void RefreshSettingsPages();
        void RefreshSettingsDevices();

        // Runs an edit, then rebuilds the list it came from on the next tick.
        void ApplyPageEdit(_In_ std::function<bool(glass::EditorController&)> edit);
        void ApplyDeviceEdit(_In_ std::function<bool(glass::EditorController&)> edit);

        // "Test MIDI 2.0 Loopback (A)" for a table entry called "Loopback A", or a line saying
        // it is not here. The layout's own name is not an endpoint name, so without this nobody
        // can tell what to point a monitor at.
        winrt::hstring DescribeResolvedDevice(_In_ std::wstring const& deviceName) const;

        // A picker over the endpoints that are present right now. Returns false when the
        // customer backed out. The name is what the layout will call the device.
        winrt::fire_and_forget ShowDevicePickerDialog(_In_ std::wstring existingName);

        winrt::fire_and_forget ShowRenamePageDialog(_In_ size_t index);
        winrt::fire_and_forget RemovePageWithConfirmation(_In_ size_t index);
        winrt::fire_and_forget ShowRenameDeviceDialog(_In_ std::wstring deviceName);

        // Asks first only when controls would be left pointing at a name that is no longer in
        // the table.
        winrt::fire_and_forget RemoveDeviceWithConfirmation(_In_ std::wstring deviceName);

        // A .syx file, read whole. False when nothing was chosen or the file is not usable.
        bool TryReadSystemExclusiveFile(_Out_ std::vector<uint8_t>& bytes);

        // ---- Edit and Try (EditorTryMode.cpp) ----

        void SetTryMode(_In_ bool tryMode);

        // Opens the connections without enabling output. Try mode and Learn share one player,
        // because creating two would open the same devices twice.
        void StartPlayerForLearning();

        // The surface only takes input in Try mode. In Edit mode every press belongs to the
        // overlay, which is what selection, handles and guides are built on.
        void ApplySurfaceInputMode();

        void OnTryValueChanged(_In_ size_t itemIndex, _In_ double value, _In_ bool isFinal);
        void OnTrySwitched(_In_ size_t itemIndex, _In_ bool isOn);
        void OnTryFeedbackMoved(_In_ uint32_t controlIndex, _In_ double value);

        void AppendMonitorRow(_In_ glass::SentMessage const& message);
        void AppendMonitorItem(_In_ glass::SentMessage const& message);
        bool IsMonitoredControl(_In_ uint32_t controlIndex) const;
        winrt::com_ptr<MonitorItem> MakeMonitorItem(_In_ glass::SentMessage const& message) const;
        void RebuildMonitorList();
        void UpdateMonitorEmptyText();

        midiapp::WindowChrome m_chrome{};

        std::wstring m_filePath{};
        glass::Theme m_theme{};
        glass::EditorController m_editor{};
        glass::SurfaceRenderer m_renderer{};

        // The inspector's preview draws one control on its own tiny deck, so a style or a color
        // is judged rather than imagined.
        glass::SurfaceRenderer m_preview{};

        glass::EditRect m_workArea{};
        double m_canvasScale{ 1.0 };

        // Fit follows the window; a typed or stepped zoom does not, or resizing the window
        // would throw away the zoom somebody just chose.
        bool m_zoomIsFit{ true };

        // ---- drag state ----

        enum class DragMode
        {
            None = 0,
            Move = 1,
            Resize = 2,
            RubberBand = 3,

            // The label's own box, which moves and sizes independently of its control.
            LabelMove = 4,
            LabelResize = 5,
        };

        DragMode m_dragMode{ DragMode::None };
        uint32_t m_dragPointerId{ 0 };
        double m_dragStartPageX{ 0.0 };
        double m_dragStartPageY{ 0.0 };
        bool m_dragMoved{ false };

        // The rubber band, so the overlay can draw it. Without something on screen a band
        // selection looks like nothing is happening until the pointer comes up.
        glass::EditRect m_band{};
        bool m_hasBand{ false };
        bool m_bandExtends{ false };

        // Clicking a control that is already selected must not clear the rest of the selection
        // until the pointer comes up without having moved, or a drag of a multiple selection
        // would collapse it on the first press.
        std::wstring m_pendingSelectId{};

        // Where the label box was when a label drag started, relative to its control.
        glass::EditRect m_labelDragStart{};
        glass::ResizeHandle m_labelDragHandle{ glass::ResizeHandle::None };
        std::wstring m_labelDragId{};

        std::vector<int32_t> m_groupChoices{};
        bool m_showAllGroups{ false };

        // The ids clicked so far while the keyboard order mode is up, in click order.
        bool m_keyboardOrderMode{ false };
        std::vector<std::wstring> m_keyboardOrderPicked{};

        // Click a palette entry, then click the page. Better with a pen or a finger, and it is
        // the path a keyboard user can take.
        bool m_hasArmedKind{ false };
        glass::ControlKind m_armedKind{ glass::ControlKind::Knob };

        // The other instinct: press a tile and drag onto the page. The TILE keeps the pointer
        // for the whole gesture, so the canvas never sees it and the move is driven from here.
        bool m_paletteDragPressed{ false };
        uint32_t m_palettePointerId{ 0 };
        glass::ControlKind m_paletteDragKind{ glass::ControlKind::Knob };
        std::wstring m_paletteDragId{};
        double m_paletteDragStartPageX{ 0.0 };
        double m_paletteDragStartPageY{ 0.0 };

        // Every droppable tile, so exactly one can be shown armed. The palette is rebuilt on
        // every search keystroke, so this is rebuilt with it.
        std::vector<std::pair<controls::Primitives::ToggleButton, glass::ControlKind>> m_paletteTiles{};

        std::vector<xaml::Shapes::Rectangle> m_handleShapes{};

        // ---- inspector state ----

        // Set while the inspector is being filled in from the document, so a SelectionChanged
        // raised by that filling does not write straight back and record an undo entry.
        bool m_updatingInspector{ false };
        int32_t m_messageIndex{ -1 };

        int32_t m_inspectorTab{ 0 };
        int32_t m_leftTab{ 0 };

        // Combo index to control id, for the two pickers that offer other controls on the
        // layout: the tempo a clock follows, and the clock a lamp follows.
        std::vector<std::wstring> m_tempoSourceIds{};
        std::vector<std::wstring> m_beatSourceIds{};

        // The same, for the devices a control can listen to. The first entry is "any device".
        std::vector<std::wstring> m_feedbackDeviceNames{};

        // ---- pane dividers ----

        bool m_draggingSplitter{ false };
        std::wstring m_splitterTag{};
        uint32_t m_splitterPointerId{ 0 };
        double m_splitterStartX{ 0.0 };
        double m_splitterStartY{ 0.0 };
        double m_splitterStartWidth{ 0.0 };
        double m_splitterStartHeight{ 0.0 };

        // What to go back to when the folded-away left pane is opened again.
        double m_leftPaneWidth{ 190.0 };

        // ---- auto save ----

        xaml::DispatcherTimer m_saveTimer{ nullptr };
        bool m_saveFailed{ false };

        // ---- Edit and Try ----

        // Checked fires while the XAML is still being built. Nothing may act on a mode change
        // until the window is up, or it runs against fields that are still null.
        bool m_loaded{ false };

        bool m_tryMode{ false };

        // ---- learn ----

        enum class LearnMode
        {
            Off = 0,

            // Arm, wiggle, done. The common case.
            One = 1,

            // Arm, then touch eight knobs in order, and eight controls fill in keyboard order.
            // This is how somebody mirrors a hardware controller in about a minute.
            Bank = 2,
        };

        void SetLearnMode(_In_ LearnMode mode);
        void UpdateLearnStatus();
        void OnLearned(_In_ glass::LearnedBinding const& learned);
        std::wstring SelectedLearnTargetId() const;
        std::vector<std::wstring> ControlsInKeyboardOrder() const;

        bool ApplyLearnedToControl(
            _In_ std::wstring const& controlId,
            _In_ glass::LearnedBinding const& learned,
            _In_ glass::LearnAcceptance const& accept);

        LearnMode m_learnMode{ LearnMode::Off };

        std::vector<std::wstring> m_learnBankOrder{};
        size_t m_learnFilled{ 0 };

        // The last capture, so a knob swept across its travel fills one control rather than a
        // hundred.
        glass::LearnedBinding m_learnLast{};
        uint64_t m_learnLastTimestamp{ 0 };
        bool m_learnHasLast{ false };

        glass::InputRouter m_input{};
        std::shared_ptr<glass::LivePlayer> m_player{};

        // ---- the monitor rail ----

        // What the app has actually sent, newest last. Capped, because a fader dragged for a
        // minute would otherwise grow this without limit while somebody was not looking at it.
        std::vector<glass::SentMessage> m_monitorRows{};

        // The stamp the elapsed column counts from. Reset by Clear, so the first row is +0.000.
        uint64_t m_monitorOrigin{ 0 };
        bool m_monitorHasOrigin{ false };

        bool m_monitorPaused{ false };
        bool m_monitorSelectedOnly{ true };
        bool m_monitorExpanded{ true };

        // What the rail goes back to when it is opened again. The comp's rail is 104 px.
        double m_monitorHeight{ 104.0 };

        // How many rows the list is showing. Kept here because asking the ListView throws once
        // its ItemsSource is set.
        uint32_t m_monitorVisibleCount{ 0 };

        // The list the rail is bound to, kept so a message can append one row rather than
        // rebuilding every row.
        foundation::Collections::IObservableVector<foundation::IInspectable> m_monitorItems{ nullptr };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

        foundation::IAsyncOperation<controls::ContentDialogResult> m_openDialog{ nullptr };
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct EditorWindow : EditorWindowT<EditorWindow, implementation::EditorWindow>
    {
    };
}
