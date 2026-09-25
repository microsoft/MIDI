// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. Everything the editor does to a layout happens here, so
// it can be driven and checked without a window.

#include <sal.h>
#include <cstdint>
#include <string>
#include <vector>

#include "LayoutModel.h"
#include "ArrangeOps.h"
#include "EditGeometry.h"
#include "RepeatPlan.h"
#include "UndoStack.h"

namespace glass
{
    // What to call an edit in the Edit menu. Resource keys, because this layer never opens a
    // resource file; the window looks them up.
    namespace EditNames
    {
        inline constexpr wchar_t Add[]{ L"EditAddControl" };
        inline constexpr wchar_t Delete[]{ L"EditDeleteControls" };
        inline constexpr wchar_t Duplicate[]{ L"EditDuplicateControls" };
        inline constexpr wchar_t Move[]{ L"EditMoveControls" };
        inline constexpr wchar_t Resize[]{ L"EditResizeControls" };
        inline constexpr wchar_t Properties[]{ L"EditControlProperties" };
        inline constexpr wchar_t Messages[]{ L"EditControlMessages" };
        inline constexpr wchar_t Arrange[]{ L"EditArrangeControls" };
        inline constexpr wchar_t ZOrder[]{ L"EditZOrder" };
        inline constexpr wchar_t Repeat[]{ L"EditRepeatControls" };
        inline constexpr wchar_t KeyboardOrder[]{ L"EditKeyboardOrder" };
        inline constexpr wchar_t PageSize[]{ L"EditPageSize" };
        inline constexpr wchar_t PageAdd[]{ L"EditAddPage" };
        inline constexpr wchar_t PageRemove[]{ L"EditRemovePage" };
        inline constexpr wchar_t PageProperties[]{ L"EditPageProperties" };
        inline constexpr wchar_t LayoutProperties[]{ L"EditLayoutProperties" };
        inline constexpr wchar_t Devices[]{ L"EditDeviceTable" };
        inline constexpr wchar_t Sequence[]{ L"EditSequence" };
    }

    // The page size is changing. What the grow and shrink dialogs hand back.
    struct PageResizeRequest
    {
        int32_t NewWidth{ 0 };
        int32_t NewHeight{ 0 };
        CanvasAnchor Anchor{ CanvasAnchor::TopLeft };
        bool ScaleContents{ false };
    };

    // One editing session. Holds the document, what is selected, what the snapping is set to,
    // and the undo stack, and is the only thing that writes to any of them.
    class EditorController
    {
    public:
        // ------------------------------------------------------------------ the document

        void Load(_In_ LayoutDocument document);

        LayoutDocument const& Document() const noexcept { return m_document; }

        // True from the first edit until MarkSaved. What the Saved chip reads.
        bool IsDirty() const noexcept { return m_dirty; }
        void MarkSaved() noexcept { m_dirty = false; }

        // ------------------------------------------------------------------ pages

        size_t PageIndex() const noexcept { return m_pageIndex; }
        void SetPageIndex(_In_ size_t index) noexcept;

        Page const* CurrentPage() const noexcept;

        bool AddPage(_In_ std::wstring const& name);
        bool RemovePage(_In_ size_t index);
        bool RenamePage(_In_ size_t index, _In_ std::wstring const& name);
        bool SetPageIsSharedBand(_In_ size_t index, _In_ bool shared);

        // ------------------------------------------------------------------ selection

        std::vector<std::wstring> const& Selection() const noexcept { return m_selection; }
        bool IsSelected(_In_ std::wstring const& id) const noexcept;

        void ClearSelection();
        void SelectOnly(_In_ std::wstring const& id);
        void ToggleSelected(_In_ std::wstring const& id);
        void AddToSelection(_In_ std::wstring const& id);
        void SelectAll();

        // The controls the warning strip is counting. Selecting them is how somebody finds the
        // eleven that ended up outside after a shrink.
        void SelectOutsidePage();

        // Every selected control in a rectangle, for a rubber band.
        void SelectInRectangle(_In_ EditRect const& area, _In_ bool add);

        std::vector<Control const*> SelectedControls() const;

        // Where a control sits counting every page in order, which is how the binding engine and
        // the surface index them. -1 when nothing has that id. The monitor rail filters on it.
        int32_t ControlIndexOf(_In_ std::wstring const& id) const;

        // ------------------------------------------------------------------ snapping

        SnapSettings const& Snap() const noexcept { return m_snap; }
        void SetGridEnabled(_In_ bool enabled) noexcept;
        void SetGridSize(_In_ double size) noexcept;

        // Alt suspends snapping while a drag is under way, without changing what the toolbar
        // says. Free-form placement is always available and never costs a setting change.
        void SetSnapSuspended(_In_ bool suspended) noexcept { m_snapSuspended = suspended; }
        bool IsSnapSuspended() const noexcept { return m_snapSuspended; }

        // ------------------------------------------------------------------ placing controls

        // Returns the id of the new control, or an empty string if the page is full.
        std::wstring AddControl(_In_ ControlKind kind, _In_ double x, _In_ double y);

        // Same, but centered on the point somebody clicked, which is what dropping feels like.
        std::wstring AddControlCentered(_In_ ControlKind kind, _In_ double centerX, _In_ double centerY);

        // Drawn out to a size with a palette tool armed, the way a drawing app places a shape.
        // One undo entry, not a placement followed by a resize.
        std::wstring AddControlInRectangle(_In_ ControlKind kind, _In_ EditRect const& area);

        // The first place on the page nothing is already using, walking the grid from the top
        // left. This is the keyboard path: somebody who cannot click the page still has to be
        // able to put a control on it, and landing every one of them on top of the last is not
        // a usable answer.
        std::wstring AddControlAtFreeSpot(_In_ ControlKind kind);

        bool DeleteSelection();
        bool DuplicateSelection();

        // ------------------------------------------------------------------ moving and resizing

        // A drag is one undo entry, not a hundred. Begin records where everything started, so
        // every update is measured from there rather than accumulating rounding.
        void BeginDrag();
        SnapOutcome UpdateDrag(_In_ double deltaX, _In_ double deltaY);
        void EndDrag();

        void BeginResize(_In_ ResizeHandle handle);
        SnapOutcome UpdateResize(_In_ double deltaX, _In_ double deltaY, _In_ bool preserveAspect);
        void EndResize();

        bool IsDragging() const noexcept { return m_dragging; }

        // Arrow keys. One pixel, or one grid cell with shift, is the caller's arithmetic.
        bool NudgeSelection(_In_ double deltaX, _In_ double deltaY);

        // Typed into the inspector, so it is exact and never snapped.
        bool SetControlBounds(
            _In_ std::wstring const& id,
            _In_ double x,
            _In_ double y,
            _In_ double width,
            _In_ double height);

        // ------------------------------------------------------------------ arranging

        bool AlignSelection(_In_ AlignEdge edge);
        bool DistributeSelection(_In_ ArrangeAxis axis);
        bool SetSelectionGap(_In_ ArrangeAxis axis, _In_ double gap);

        // Which controls are drawn over which. A page is painted in the order its controls are
        // stored, so this reorders that list; nothing else about a control changes, and the
        // keyboard order is a separate idea that this leaves alone.
        bool ChangeZOrder(_In_ ZOrderMove move);

        // What the spacing pills show. Empty when fewer than two are selected.
        std::vector<double> SelectionGaps(_In_ ArrangeAxis axis) const;

        bool RepeatSelection(_In_ RepeatOptions const& options);

        // ------------------------------------------------------------------ control properties

        bool SetControlLabel(_In_ std::wstring const& id, _In_ std::wstring const& label);
        bool SetControlKind(_In_ std::wstring const& id, _In_ ControlKind kind);
        bool SetControlHueSlot(_In_ std::wstring const& id, _In_ int32_t slot);
        bool SetControlAspectLocked(_In_ std::wstring const& id, _In_ bool locked);

        // Where this control disagrees with its theme. Almost every control stays at UseTheme,
        // which is what keeps a theme change a six color operation rather than a redesign.
        bool SetControlStyle(_In_ std::wstring const& id, _In_ ControlStyleOverride style);
        bool SetControlLabelPlacement(_In_ std::wstring const& id, _In_ LabelPlacementOverride placement);
        bool SetControlShowValue(_In_ std::wstring const& id, _In_ ShowValueOverride showValue);
        bool SetControlDefaultValue(_In_ std::wstring const& id, _In_ double value);
        bool SetControlReturnsToDefault(_In_ std::wstring const& id, _In_ bool returns);
        bool SetControlSendsValueOnStart(_In_ std::wstring const& id, _In_ bool sends);
        bool SetControlSendInterval(_In_ std::wstring const& id, _In_ int32_t milliseconds);
        bool SetControlPickup(_In_ std::wstring const& id, _In_ PickupMode pickup);

        // ------------------------------------------------------------------ what a control sends

        bool AddMessage(_In_ std::wstring const& id);
        bool RemoveMessage(_In_ std::wstring const& id, _In_ size_t index);
        bool SetMessage(_In_ std::wstring const& id, _In_ size_t index, _In_ ControlMessage const& message);
        bool SetFeedback(_In_ std::wstring const& id, _In_ FeedbackBinding const& feedback);

        // ------------------------------------------------------------------ keyboard order

        // The order a screen reader walks, and the order a bank learn fills. A hidden ordering
        // is one nobody can fix, so it is a property of the layout rather than of the tree.
        bool SetKeyboardOrder(_In_ std::wstring const& id, _In_ int32_t order);

        // Renumbers every control on the page from one, keeping the order they are already in.
        bool RenumberKeyboardOrder();

        // Reading order: left to right, top to bottom, in bands a control's height deep.
        bool SortKeyboardOrderByPosition();

        // ------------------------------------------------------------------ layout properties

        bool SetLayoutName(_In_ std::wstring const& name);
        bool SetLayoutDescription(_In_ std::wstring const& description);
        bool SetThemeName(_In_ std::wstring const& themeName);
        bool SetSuppressAllStartupValues(_In_ bool suppress);

        // Growing asks where the existing controls should sit; shrinking offers to scale them.
        // Neither ever clamps a control inside the page, because clamping makes a resize
        // unrecoverable.
        bool ResizePage(_In_ PageResizeRequest const& request);

        // How many controls a resize would leave outside, worked out before anything changes.
        size_t CountOutsideAfterResize(_In_ PageResizeRequest const& request) const;

        // Where the page sits inside the work area the canvas draws.
        EditRect WorkArea() const;

        std::vector<Control const*> ControlsOutsidePage() const;

        // ------------------------------------------------------------------ devices

        bool AddDevice(_In_ DeviceEntry const& device);
        bool RemoveDevice(_In_ std::wstring const& name);
        bool RenameDevice(_In_ std::wstring const& oldName, _In_ std::wstring const& newName);

        // ------------------------------------------------------------------ sequences

        // A layout-wide list, not a property of one control, so two buttons can play the same
        // sequence and a change reaches both. Returns the name it ended up with, which is not
        // the one asked for when that name was taken.
        std::wstring AddSequence(_In_ std::wstring const& name);

        // Replaces one sequence whole. The editor builds the new steps in a dialog and hands
        // them over at the end, so a canceled dialog leaves nothing behind.
        bool SetSequence(_In_ std::wstring const& name, _In_ glass::Sequence const& sequence);

        bool RemoveSequence(_In_ std::wstring const& name);

        // ------------------------------------------------------------------ undo

        bool CanUndo() const noexcept { return m_undo.CanUndo(); }
        bool CanRedo() const noexcept { return m_undo.CanRedo(); }
        std::wstring UndoName() const { return m_undo.UndoName(); }
        std::wstring RedoName() const { return m_undo.RedoName(); }
        size_t UndoDepth() const noexcept { return m_undo.UndoDepth(); }

        bool Undo();
        bool Redo();

    private:
        Page* MutablePage() noexcept;
        Control* MutableControl(_In_ std::wstring const& id) noexcept;

        void Commit(_In_ wchar_t const* name);
        void CommitCoalesced(_In_ wchar_t const* name, _In_ std::wstring const& key);

        // Anything an undo or a page change can invalidate.
        void DropSelectionOfMissingControls();

        std::vector<EditRect> OtherRects(_In_ std::vector<std::wstring> const& excluding) const;
        EditRect PageRect() const noexcept;
        SnapSettings EffectiveSnap() const noexcept;

        LayoutDocument m_document{};
        UndoStack m_undo{};

        size_t m_pageIndex{ 0 };
        std::vector<std::wstring> m_selection{};

        SnapSettings m_snap{};
        bool m_snapSuspended{ false };

        bool m_dirty{ false };

        // Where the selection was when the gesture started. Every update is measured from here.
        struct DragOrigin
        {
            std::wstring Id{};
            EditRect Rect{};
        };

        std::vector<DragOrigin> m_dragOrigins{};
        bool m_dragging{ false };
        ResizeHandle m_resizeHandle{ ResizeHandle::None };
    };
}
