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
#include <optional>
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
        inline constexpr wchar_t Paste[]{ L"EditPasteControls" };
        inline constexpr wchar_t Cut[]{ L"EditCutControls" };
        inline constexpr wchar_t Group[]{ L"EditGroupControls" };
        inline constexpr wchar_t Ungroup[]{ L"EditUngroupControls" };
        inline constexpr wchar_t RenameGroup[]{ L"EditRenameGroup" };
        inline constexpr wchar_t Lock[]{ L"EditLockControls" };
        inline constexpr wchar_t Unlock[]{ L"EditUnlockControls" };
    }

    // The page size is changing. What the grow and shrink dialogs hand back.
    struct PageResizeRequest
    {
        int32_t NewWidth{ 0 };
        int32_t NewHeight{ 0 };
        CanvasAnchor Anchor{ CanvasAnchor::TopLeft };
        bool ScaleContents{ false };
    };

    // One line of the outline. A group is a heading with its members after it, one step in, so
    // the list shows what moves together.
    struct OutlineRow
    {
        // A control's id, or a group's id for its heading.
        std::wstring Id{};
        bool IsGroupHeading{ false };

        // One for a member of a group, zero for everything else.
        int32_t Depth{ 0 };

        // Counted from one in the order the groups are listed, so two groups can be told apart
        // by name. Zero for a control on its own.
        int32_t GroupNumber{ 0 };

        // How many controls a heading stands for.
        size_t MemberCount{ 0 };

        // What the group is called, on its heading and on each member. Empty when nobody named
        // it, and the window shows the number instead.
        std::wstring GroupName{};
    };

    // A device, a group and a channel, for several controls at once. A field is empty where the
    // controls disagree, or where an edit leaves it alone.
    struct DestinationFields
    {
        std::optional<std::wstring> DeviceName{};
        std::optional<int32_t> GroupIndex{};
        std::optional<int32_t> ChannelIndex{};
    };

    // What several controls agree on, and how many rows the answer was worked out from: the ones
    // that go to a device, and the ones among them that carry a channel.
    struct SharedDestination
    {
        DestinationFields Fields{};
        size_t DeviceRows{ 0 };
        size_t ChannelRows{ 0 };
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

        // Moves everything on one page to the end of another, then removes the page it emptied.
        // A page full of work should never be a choice between keeping the page and keeping the
        // controls.
        bool MoveControlsAndRemovePage(_In_ size_t index, _In_ size_t destinationIndex);
        bool RenamePage(_In_ size_t index, _In_ std::wstring const& name);
        bool SetPageIsSharedBand(_In_ size_t index, _In_ bool shared);

        // The order of the pages is the order of the tab strip on the surface, so reordering
        // them here is how somebody puts the page they reach for first at the front.
        bool MovePage(_In_ size_t index, _In_ bool up);

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

        // The outer edges of everything selected. Empty when nothing is.
        EditRect SelectionBounds() const;

        // ------------------------------------------------------------------ groups

        // Controls that select and move as one. Flat rather than nested: grouping controls that
        // are already grouped makes one bigger group, which is what somebody means when they
        // pick several things and press Group.
        bool GroupSelection();
        bool UngroupSelection();

        // Every member of one group is selected, and nothing else is.
        bool SelectionIsOneGroup() const;
        bool SelectionHasGroup() const;

        // The group the selection is, or empty when it is not exactly one whole group.
        std::wstring SelectedGroupId() const;

        // What somebody called a group on the current page. Empty when nobody named it.
        std::wstring GroupName(_In_ std::wstring const& groupId) const;

        // Where a group falls in the outline, counted from one: the number it goes by until
        // somebody names it. Zero when this page lists no such group.
        int32_t GroupNumber(_In_ std::wstring const& groupId) const;

        // Typing arrives a character at a time, so a run of it is one undo entry. An empty name
        // takes the group back to its number.
        bool SetGroupName(_In_ std::wstring const& groupId, _In_ std::wstring const& name);

        // A click on the canvas picks the whole group a control is in. The outline, and a second
        // click on a group that is already selected, pick the one member.
        void SelectGroupOf(_In_ std::wstring const& id);
        void ToggleGroupOf(_In_ std::wstring const& id);
        void ExpandSelectionToGroups();

        // A group's heading in the outline picks every member.
        void AddGroupToSelection(_In_ std::wstring const& groupId);
        bool IsWholeGroupSelected(_In_ std::wstring const& groupId) const;

        // The current page as the outline lists it. Reading order is the keyboard order, and a
        // group sits where its first member falls in it. A group with one member left is listed
        // as a control on its own, because nothing moves with it.
        std::vector<OutlineRow> OutlineRows() const;

        // ------------------------------------------------------------------ locking

        // Everything selected, locked or unlocked in one undo step. A locked control cannot be
        // picked on the page, moved or resized. The outline still selects it, which is how it
        // gets unlocked again.
        bool SetSelectionLocked(_In_ bool locked);

        // Something is selected and all of it is locked.
        bool SelectionIsLocked() const;

        // Any of the selection is locked, which is what stops a resize.
        bool SelectionHasLocked() const;

        // ------------------------------------------------------------------ several at once

        // Every edit between these two is one undo entry, however many controls it touched.
        // For changing one property on every selected control.
        void BeginEditBatch();
        void EndEditBatch();

        // Typed into the inspector with several controls selected: a new position moves them
        // all, a new size scales them all within the new box.
        bool SetSelectionBounds(_In_ double x, _In_ double y, _In_ double width, _In_ double height);

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

        // The first place on the page nothing is already using, walking the grid from the top
        // left. This is the keyboard path: somebody who cannot click the page still has to be
        // able to put a control on it, and landing every one of them on top of the last is not
        // a usable answer.
        std::wstring AddControlAtFreeSpot(_In_ ControlKind kind);

        bool DeleteSelection();
        bool DuplicateSelection();

        // ------------------------------------------------------------------ the clipboard

        // The selection as clipboard text: a layout of its own, one page holding copies of the
        // selected controls, plus the devices and sequences they name so a paste into another
        // layout still knows what those names mean. Empty when nothing is selected.
        std::wstring CopySelection() const;

        // Copied, then removed. One undo entry, named for what the customer did.
        std::wstring CutSelection();

        // Controls from CopySelection, added to this page with new ids and selected. Read with
        // the same reader as a layout file, because the clipboard is exactly as untrusted as a
        // stranger's file. A paste over the originals lands a step down and across, so it never
        // hides under what it came from. False when the text is not a copy of controls, or the
        // page cannot take that many more.
        bool PasteControls(_In_ std::wstring const& text);

        // Plain text becomes a text control showing it, centered on the point given.
        bool PasteText(_In_ std::wstring const& text, _In_ double centerX, _In_ double centerY);

        // ------------------------------------------------------------------ moving and resizing

        // A drag is one undo entry, not a hundred. Begin records where everything started, so
        // every update is measured from there rather than accumulating rounding.
        void BeginDrag();
        // A straight line, when asked: across or down, whichever way the pointer has traveled
        // further from where the drag started. Decided again on every move, so a drag that
        // sets off sideways and then turns downward follows the turn.
        SnapOutcome UpdateDrag(_In_ double deltaX, _In_ double deltaY, _In_ bool straightLine = false);
        void EndDrag();

        void BeginResize(_In_ ResizeHandle handle);
        SnapOutcome UpdateResize(_In_ double deltaX, _In_ double deltaY, _In_ bool preserveAspect);
        void EndResize();

        // Closes whatever run of coalesced edits is open. A label drag has no Begin of its own,
        // because nothing needs recording: the box it started at is held by the caller.
        void EndCoalescing() noexcept;

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

        // A group is lined up and spaced as one block, so a channel strip keeps its shape. A
        // selection that is all one group is taken control by control instead, because lining
        // up the members of one group with each other is the reason to pick just them.
        bool AlignSelection(_In_ AlignEdge edge);
        bool DistributeSelection(_In_ ArrangeAxis axis);
        bool SetSelectionGap(_In_ ArrangeAxis axis, _In_ double gap);

        // The same for controls named by id rather than by the selection. A gap typed into the
        // canvas is applied when the box closes, and a click elsewhere can change the selection
        // first.
        bool SpaceControls(_In_ std::vector<std::wstring> const& ids, _In_ ArrangeAxis axis, _In_ double gap);

        // How many blocks the selection is: what Align needs two of and Distribute three.
        size_t SelectionBlockCount() const;

        // Which controls are drawn over which. A page is painted in the order its controls are
        // stored, so this reorders that list; nothing else about a control changes, and the
        // keyboard order is a separate idea that this leaves alone.
        bool ChangeZOrder(_In_ ZOrderMove move);

        // The gaps between the selected blocks along one axis, in position order.
        std::vector<double> SelectionGaps(_In_ ArrangeAxis axis) const;

        // What the spacing pills show: the way the selection runs, its blocks and their gaps.
        // Nothing when fewer than two blocks are selected or they overlap both ways.
        std::optional<SpacingReadout> SelectionSpacing() const;

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
        bool SetControlLabelStyle(_In_ std::wstring const& id, _In_ LabelStyle const& style);

        // The label's own rectangle, dragged with handles on the canvas. Coalesced, so a drag is
        // one undo entry rather than one per pointer move.
        bool SetControlLabelBox(
            _In_ std::wstring const& id,
            _In_ double x,
            _In_ double y,
            _In_ double width,
            _In_ double height);

        // Back to whatever the placement rule says. Not coalesced: this is a decision, not a drag.
        bool ClearControlLabelBox(_In_ std::wstring const& id);
        bool SetControlShowValue(_In_ std::wstring const& id, _In_ ShowValueOverride showValue);
        bool SetControlDefaultValue(_In_ std::wstring const& id, _In_ double value);
        bool SetControlReturnsToDefault(_In_ std::wstring const& id, _In_ bool returns);

        // A knob or fader that works like a pan control. Turning it on also moves a control
        // resting at the bottom of its travel to the middle, in the same undo step, because the
        // middle is where a pan control rests.
        bool SetControlLightsFromCenter(_In_ std::wstring const& id, _In_ bool fromCenter);
        bool SetControlSendsValueOnStart(_In_ std::wstring const& id, _In_ bool sends);
        bool SetControlSendInterval(_In_ std::wstring const& id, _In_ int32_t milliseconds);
        bool SetControlPickup(_In_ std::wstring const& id, _In_ PickupMode pickup);

        // Which way a finger drags a knob up.
        bool SetControlDrag(_In_ std::wstring const& id, _In_ DragAxis drag);

        // The marks across the travel, and whether the value at each stop is printed beside
        // them.
        bool SetControlTicks(_In_ std::wstring const& id, _In_ TickMarks const& ticks);
        bool SetControlShowDetentValues(_In_ std::wstring const& id, _In_ bool show);
        bool SetControlVelocityFromTouch(_In_ std::wstring const& id, _In_ bool fromTouch);

        // The picture or video an image control shows, and the fill behind a grouping panel.
        // Coalesced when asked, so dragging the crop box is one undo step, closed by
        // EndCoalescing when the drag ends.
        bool SetControlPicture(
            _In_ std::wstring const& id,
            _In_ Picture const& picture,
            _In_ bool coalesce = false);

        // A switch's positions. Adding one gives it a message row of its own; removing one takes
        // its rows with it and moves the rows after it down by one, so every row still names
        // the position it meant.
        bool SetSwitchPositionName(_In_ std::wstring const& id, _In_ size_t index, _In_ std::wstring const& name);
        bool AddSwitchPosition(_In_ std::wstring const& id);
        bool RemoveSwitchPosition(_In_ std::wstring const& id, _In_ size_t index);

        bool SetControlKeyboard(_In_ std::wstring const& id, _In_ KeyboardSpec const& keyboard);
        bool SetControlClock(_In_ std::wstring const& id, _In_ ClockSpec const& clock);
        bool SetControlLfo(_In_ std::wstring const& id, _In_ LfoSpec const& lfo);

        // A step sequencer's own settings: its rate, gate, swing and direction, and how a press
        // starts it. The steps themselves are left as they are.
        bool SetStepsSettings(_In_ std::wstring const& id, _In_ StepsSpec const& settings);

        // How many steps it has. New steps copy the last one, so a longer pattern carries on
        // from where it ended rather than filling up with the same default note.
        bool SetStepCount(_In_ std::wstring const& id, _In_ int32_t count);

        // One step. Coalesced per step while a number is being typed or dragged.
        bool SetStep(
            _In_ std::wstring const& id,
            _In_ int32_t index,
            _In_ SequencerStep const& step,
            _In_ bool coalesce = false);

        bool SetControlTurntable(_In_ std::wstring const& id, _In_ TurntableSpec const& turntable);

        // A printed line's thickness, color and ends.
        bool SetControlLine(_In_ std::wstring const& id, _In_ LineSpec const& line);

        // The pads on a note pad or hex pad control: how many, how big, what they play, how they
        // are colored and what a slide between them does.
        bool SetControlPads(_In_ std::wstring const& id, _In_ PadGridSpec const& pads);

        // The second axis's starting value, for an XY pad and a joystick.
        bool SetControlDefaultValueY(_In_ std::wstring const& id, _In_ double value);

        // ------------------------------------------------------------------ what a control sends

        bool AddMessage(_In_ std::wstring const& id);
        bool RemoveMessage(_In_ std::wstring const& id, _In_ size_t index);
        bool SetMessage(_In_ std::wstring const& id, _In_ size_t index, _In_ ControlMessage const& message);
        bool SetFeedback(_In_ std::wstring const& id, _In_ FeedbackBinding const& feedback);

        // Where several controls send, read and set in one go. Every row that goes to a device
        // takes the device and group, and every row that carries a channel takes the channel.
        // A page change or a sequence goes nowhere, so it is left alone. One undo entry.
        SharedDestination SharedSendDestination(_In_ std::vector<std::wstring> const& ids) const;
        bool SetSendDestination(_In_ std::vector<std::wstring> const& ids, _In_ DestinationFields const& change);

        // The same for where they listen. Only a control that follows what arrives counts, and
        // an empty device name means any device.
        SharedDestination SharedListenDestination(_In_ std::vector<std::wstring> const& ids) const;
        bool SetListenDestination(_In_ std::vector<std::wstring> const& ids, _In_ DestinationFields const& change);

        // ------------------------------------------------------------------ keyboard order

        // The order a screen reader walks, and the order a bank learn fills. A hidden ordering
        // is one nobody can fix, so it is a property of the layout rather than of the tree.
        bool SetKeyboardOrder(_In_ std::wstring const& id, _In_ int32_t order);

        // Renumbers every control on the page from one, keeping the order they are already in.
        bool RenumberKeyboardOrder();

        // Reading order: left to right, top to bottom, in bands a control's height deep.
        bool SortKeyboardOrderByPosition();

        // The order the customer clicked, applied in one go. Controls that were never clicked
        // keep their relative order and follow the ones that were, so leaving the job half done
        // is a partial answer rather than a wrecked one.
        bool SetKeyboardOrderFromList(_In_ std::vector<std::wstring> const& idsInOrder);

        // ------------------------------------------------------------------ layout properties

        bool SetLayoutName(_In_ std::wstring const& name);
        bool SetLayoutDescription(_In_ std::wstring const& description);

        // The customer picked a theme out of the gallery. The layout stops carrying one of its
        // own, so an improvement to the shipped theme still reaches this layout.
        bool ChooseTheme(_In_ Theme const& theme);

        // The customer changed one of the theme's own numbers. From here on the layout carries
        // the theme inside itself, because there is no file anywhere that says what it now is.
        bool SetOwnTheme(_In_ Theme const& theme);

        bool SetSuppressAllStartupValues(_In_ bool suppress);
        bool SetPublishesVirtualDevice(_In_ bool publishes);
        bool SetScaleMode(_In_ ScaleMode mode, _In_ double customPercent);
        bool SetFullScreenButtonCorner(_In_ ScreenCorner corner);
        bool SetToolbarWindow(_In_ bool toolbar);
        bool SetAlwaysOnTop(_In_ bool onTop);
        bool SetSeeThrough(_In_ bool seeThrough);
        bool SetTempoSource(_In_ TempoSource const& tempo);

        // The picture behind the whole surface. The name is a bare file name beside the layout,
        // never a path: a layout is untrusted input, and a path in it is a way to make this app
        // open a file somewhere else on the PC. Pass an empty name to take the picture away.
        // The fit and how strongly it shows come from the same dialog, so all three are one step.
        bool SetBackgroundImage(_In_ std::wstring const& fileName, _In_ BackgroundFit fit, _In_ double opacity);

        // Growing asks where the existing controls should sit; shrinking offers to scale them.
        // Neither ever clamps a control inside the page, because clamping makes a resize
        // unrecoverable.
        bool ResizePage(_In_ PageResizeRequest const& request);

        // How many controls a resize would leave outside, worked out before anything changes.
        size_t CountOutsideAfterResize(_In_ PageResizeRequest const& request) const;

        std::vector<Control const*> ControlsOutsidePage() const;

        // ------------------------------------------------------------------ devices

        bool AddDevice(_In_ DeviceEntry const& device);
        bool RemoveDevice(_In_ std::wstring const& name);
        bool RenameDevice(_In_ std::wstring const& oldName, _In_ std::wstring const& newName);

        // Points an entry that is already in the table at different hardware, without touching
        // the name, so every control that used it follows.
        bool SetDeviceMatch(
            _In_ std::wstring const& name,
            _In_ midiapp::EndpointMatch const& match,
            _In_ midiapp::EndpointMatchMode mode);

        bool SetDeviceMatchMode(_In_ std::wstring const& name, _In_ midiapp::EndpointMatchMode mode);

        // Changes how the layout talks to a device, and brings every row sent there along. A row
        // that already is a Mackie Control function becomes it, and a function becomes the plain
        // message it stands for when the device stops speaking Mackie Control.
        bool SetDeviceProtocol(_In_ std::wstring const& name, _In_ DeviceProtocol protocol);

        // How many controls, across every page, send to this entry. The device list shows it so
        // that removing an entry is not a guess.
        size_t CountControlsUsingDevice(_In_ std::wstring const& name) const;

        // ------------------------------------------------------------------ sequences

        // A layout-wide list, not a property of one control, so two buttons can play the same
        // sequence and a change reaches both. Returns the name it ended up with, which is not
        // the one asked for when that name was taken.
        std::wstring AddSequence(_In_ std::wstring const& name);

        // Replaces one sequence whole. The editor builds the new steps in a dialog and hands
        // them over at the end, so a canceled dialog leaves nothing behind.
        bool SetSequence(_In_ std::wstring const& name, _In_ glass::Sequence const& sequence);

        bool RemoveSequence(_In_ std::wstring const& name);

        // How many controls, across every page, have a row that plays this sequence.
        size_t CountControlsUsingSequence(_In_ std::wstring const& name) const;

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

        // Each control keeps its place within the box as the box goes from one rectangle to
        // another. A control that keeps its shape is scaled evenly and stays centered where its
        // middle went, so a row of knobs does not turn into a row of ellipses.
        void PlaceWithin(
            _In_ std::vector<DragOrigin> const& origins,
            _In_ EditRect const& from,
            _In_ EditRect const& to);

        std::vector<DragOrigin> m_dragOrigins{};
        bool m_dragging{ false };
        ResizeHandle m_resizeHandle{ ResizeHandle::None };

        // The box around several controls being resized together.
        EditRect m_resizeBounds{};

        int32_t m_batchDepth{ 0 };
        uint64_t m_batchNumber{ 0 };
    };

    // An edit batch for as long as it lives, so an early return cannot leave one open.
    class EditBatch
    {
    public:
        explicit EditBatch(_Inout_ EditorController& editor) : m_editor(editor) { m_editor.BeginEditBatch(); }
        ~EditBatch() { m_editor.EndEditBatch(); }

        EditBatch(EditBatch const&) = delete;
        EditBatch& operator=(EditBatch const&) = delete;

    private:
        EditorController& m_editor;
    };
}
