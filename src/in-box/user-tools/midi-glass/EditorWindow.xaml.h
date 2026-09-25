// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "EditorWindow.g.h"

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
        void OnRenameLayoutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
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

        // ---- accelerators ----

        void OnUndoAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnRedoAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnSaveAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnDuplicateAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnSelectAllAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnDeleteAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnEscapeAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnTryAccelerator(xaml::Input::KeyboardAccelerator const& sender, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

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

        void OnSendOnStartToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnDefaultValueChanged(foundation::IInspectable const& sender, controls::Primitives::RangeBaseValueChangedEventArgs const& args);
        void OnSendIntervalChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnPickupChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnKeyboardOrderChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);

    private:
        void OnWindowClosed(foundation::IInspectable const& sender, xaml::WindowEventArgs const& args);

        // ---- canvas (EditorCanvas.cpp) ----

        void BuildPage();
        void RebuildSurface();
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
        void InitializeSplitters();

        // Work area coordinates to page coordinates. The work area starts at a negative offset,
        // so this is not a no-op even at a scale of one.
        glass::EditRect WorkArea() const noexcept { return m_workArea; }
        double PointToPageX(_In_ double x) const noexcept { return x + m_workArea.X; }
        double PointToPageY(_In_ double y) const noexcept { return y + m_workArea.Y; }

        std::wstring HitTest(_In_ double pageX, _In_ double pageY) const;
        glass::ResizeHandle HitTestHandle(_In_ double pageX, _In_ double pageY) const;

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
        void SyncStyleSegments(_In_ glass::ControlStyleOverride style);
        void RefreshPreview(_In_ glass::Control const& control);

        void SelectInspectorTab(_In_ int32_t index);
        void SelectLeftTab(_In_ int32_t index);

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
        winrt::fire_and_forget ShowRenameDialog();

        // ---- Edit and Try (EditorTryMode.cpp) ----

        void SetTryMode(_In_ bool tryMode);

        // The surface only takes input in Try mode. In Edit mode every press belongs to the
        // overlay, which is what selection, handles and guides are built on.
        void ApplySurfaceInputMode();

        void OnTryValueChanged(_In_ size_t itemIndex, _In_ double value, _In_ bool isFinal);
        void OnTrySwitched(_In_ size_t itemIndex, _In_ bool isOn);
        void OnTryFeedbackMoved(_In_ uint32_t controlIndex, _In_ double value);

        void AppendMonitorRow(_In_ glass::SentMessage const& message);
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

        // How many rows the list is showing. Kept here because asking the ListView throws once
        // its ItemsSource is set.
        uint32_t m_monitorVisibleCount{ 0 };

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
