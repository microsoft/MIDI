// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "EditorControllerTests.h"

#include "EditorController.h"
#include "ControlFactory.h"
#include "InputRules.h"
#include "LayoutSerializer.h"

#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    constexpr double Tolerance = 0.0001;

    void VerifyNear(_In_ double expected, _In_ double actual)
    {
        VERIFY_IS_LESS_THAN(std::abs(expected - actual), Tolerance);
    }

    glass::LayoutDocument EmptyDocument()
    {
        glass::LayoutDocument document{};

        document.Name = L"Test";
        document.PageWidth = 1280;
        document.PageHeight = 800;

        glass::DeviceEntry device{};
        device.Name = L"Synth";
        document.Devices.push_back(device);

        glass::Page page{};
        page.Id = glass::LayoutDocument::NewId();
        page.Name = L"Page 1";

        document.Pages.push_back(std::move(page));

        return document;
    }

    glass::EditorController LoadedController()
    {
        glass::EditorController controller{};
        controller.Load(EmptyDocument());

        return controller;
    }

    glass::Control const* ControlAt(_In_ glass::EditorController const& controller, _In_ size_t index)
    {
        auto const* const page = controller.CurrentPage();

        VERIFY_IS_NOT_NULL(page);
        VERIFY_IS_GREATER_THAN(page->Controls.size(), index);

        return &page->Controls[index];
    }

    size_t ControlCount(_In_ glass::EditorController const& controller)
    {
        auto const* const page = controller.CurrentPage();

        return page == nullptr ? 0 : page->Controls.size();
    }

    // A control placed exactly, with snapping out of the way, so a test measures what it means
    // to measure rather than what the grid did to it. Setting bounds that already match is a
    // no-op by design, so the result is checked rather than the return value.
    std::wstring PlaceExactly(
        _Inout_ glass::EditorController& controller,
        _In_ glass::ControlKind kind,
        _In_ double x,
        _In_ double y,
        _In_ double width,
        _In_ double height)
    {
        auto const id = controller.AddControl(kind, x, y);

        VERIFY_IS_FALSE(id.empty());

        controller.SetControlBounds(id, x, y, width, height);

        auto const* const placed = controller.Document().FindControl(id);

        VERIFY_IS_NOT_NULL(placed);
        VerifyNear(x, placed->X);
        VerifyNear(y, placed->Y);
        VerifyNear(width, placed->Width);
        VerifyNear(height, placed->Height);

        return id;
    }
}

// ---- undo ----

void EditorControllerTests::NothingToUndoOnAFreshDocument()
{
    auto controller = LoadedController();

    VERIFY_IS_FALSE(controller.CanUndo());
    VERIFY_IS_FALSE(controller.CanRedo());
    VERIFY_ARE_EQUAL(size_t{ 0 }, controller.UndoDepth());
}

void EditorControllerTests::AnEditCanBeTakenBack()
{
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);

    VERIFY_ARE_EQUAL(size_t{ 1 }, ControlCount(controller));
    VERIFY_IS_TRUE(controller.CanUndo());

    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_ARE_EQUAL(size_t{ 0 }, ControlCount(controller));
}

void EditorControllerTests::RedoPutsItBack()
{
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_TRUE(controller.CanRedo());
    VERIFY_IS_TRUE(controller.Redo());

    VERIFY_ARE_EQUAL(size_t{ 1 }, ControlCount(controller));
}

void EditorControllerTests::ANewEditClearsTheRedoBranch()
{
    // A redo after a new edit would reinstate something built on a document that no longer
    // exists.
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_TRUE(controller.CanRedo());

    controller.AddControl(glass::ControlKind::Knob, 300, 100);

    VERIFY_IS_FALSE(controller.CanRedo());
}

void EditorControllerTests::ADragIsOneEntry()
{
    auto controller = LoadedController();

    auto const id = controller.AddControl(glass::ControlKind::Fader, 100, 100);
    auto const depth = controller.UndoDepth();

    controller.SelectOnly(id);
    controller.BeginDrag();

    for (int step = 1; step <= 50; ++step)
    {
        controller.UpdateDrag(step * 2.0, 0.0);
    }

    controller.EndDrag();

    VERIFY_ARE_EQUAL(depth + 1, controller.UndoDepth());

    // And taking it back returns the control to where the drag started.
    VERIFY_IS_TRUE(controller.Undo());
    VerifyNear(104.0, ControlAt(controller, 0)->X);
}

void EditorControllerTests::TwoDragsAreTwoEntries()
{
    auto controller = LoadedController();

    auto const id = controller.AddControl(glass::ControlKind::Fader, 100, 100);
    auto const depth = controller.UndoDepth();

    controller.SelectOnly(id);

    controller.BeginDrag();
    controller.UpdateDrag(40.0, 0.0);
    controller.EndDrag();

    controller.BeginDrag();
    controller.UpdateDrag(40.0, 0.0);
    controller.EndDrag();

    VERIFY_ARE_EQUAL(depth + 2, controller.UndoDepth());
}

void EditorControllerTests::WritingBackAnUnchangedValueIsNotAnEdit()
{
    // The inspector fills itself in from the document, and a XAML control raises its changed
    // event when it is filled in. Without this, showing a control's properties records an edit.
    auto controller = LoadedController();

    auto const id = controller.AddControl(glass::ControlKind::Fader, 100, 100);
    auto const depth = controller.UndoDepth();

    auto const* const control = ControlAt(controller, 0);

    VERIFY_IS_FALSE(controller.SetControlBounds(id, control->X, control->Y, control->Width, control->Height));
    VERIFY_IS_FALSE(controller.SetMessage(id, 0, control->Messages[0]));
    VERIFY_IS_FALSE(controller.SetFeedback(id, control->Feedback));
    VERIFY_IS_FALSE(controller.SetControlLabel(id, control->Label));
    VERIFY_IS_FALSE(controller.SetControlKind(id, control->Kind));
    VERIFY_IS_FALSE(controller.SetControlHueSlot(id, control->HueSlot));
    VERIFY_IS_FALSE(controller.SetKeyboardOrder(id, control->KeyboardOrder));

    VERIFY_ARE_EQUAL(depth, controller.UndoDepth());
}

void EditorControllerTests::ShowingAControlDoesNotThrowAwayTheRedoBranch()
{
    // Found by driving the editor: after an undo, the redo button was dead, because refreshing
    // the inspector wrote every field back and one of those writes counted as an edit.
    auto controller = LoadedController();

    auto const id = controller.AddControl(glass::ControlKind::Fader, 100, 100);

    VERIFY_IS_TRUE(controller.SetControlBounds(id, 456, 100, 40, 180));
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_TRUE(controller.CanRedo());

    auto const* const control = ControlAt(controller, 0);

    controller.SetControlBounds(id, control->X, control->Y, control->Width, control->Height);
    controller.SetMessage(id, 0, control->Messages[0]);
    controller.SetControlLabel(id, control->Label);

    VERIFY_IS_TRUE(controller.CanRedo());

    VERIFY_IS_TRUE(controller.Redo());
    VerifyNear(456.0, ControlAt(controller, 0)->X);
}

void EditorControllerTests::UndoNamesTheEdit()
{
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);

    VERIFY_ARE_EQUAL(std::wstring{ glass::EditNames::Add }, controller.UndoName());
}

void EditorControllerTests::TheStackIsBounded()
{
    auto controller = LoadedController();

    for (int index = 0; index < static_cast<int>(glass::UndoStack::MaximumDepth) + 20; ++index)
    {
        controller.AddControl(glass::ControlKind::Lamp, 8.0 * index, 8.0);
    }

    VERIFY_ARE_EQUAL(glass::UndoStack::MaximumDepth, controller.UndoDepth());
}

void EditorControllerTests::UndoRestoresEveryControlADeleteTookOut()
{
    // The classic undo defect is the one that restores nine of the ten things an edit touched.
    auto controller = LoadedController();

    for (int index = 0; index < 10; ++index)
    {
        controller.AddControl(glass::ControlKind::Pad, 64.0 * index, 64.0);
    }

    controller.SelectAll();
    VERIFY_IS_TRUE(controller.DeleteSelection());
    VERIFY_ARE_EQUAL(size_t{ 0 }, ControlCount(controller));

    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_ARE_EQUAL(size_t{ 10 }, ControlCount(controller));
}

// ---- placing ----

void EditorControllerTests::ADroppedControlSendsSomething()
{
    // A control that sends nothing is one somebody has to visit the MIDI tab for before it does
    // anything at all.
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);

    auto const* const control = ControlAt(controller, 0);

    VERIFY_ARE_EQUAL(size_t{ 1 }, control->Messages.size());
    VERIFY_IS_TRUE(control->Messages[0].Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(std::wstring{ L"Synth" }, control->Messages[0].DeviceName);
}

void EditorControllerTests::ADroppedDisplayControlSendsNothing()
{
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Label, 100, 100);

    VERIFY_ARE_EQUAL(size_t{ 0 }, ControlAt(controller, 0)->Messages.size());
}

void EditorControllerTests::EightDroppedFadersDoNotAllUseControllerOne()
{
    auto controller = LoadedController();

    for (int index = 0; index < 8; ++index)
    {
        controller.AddControl(glass::ControlKind::Fader, 64.0 * index, 100);
    }

    std::vector<uint32_t> numbers{};

    for (size_t index = 0; index < 8; ++index)
    {
        numbers.push_back(ControlAt(controller, index)->Messages[0].Number);
    }

    for (size_t index = 1; index < numbers.size(); ++index)
    {
        VERIFY_ARE_NOT_EQUAL(numbers[index - 1], numbers[index]);
    }
}

void EditorControllerTests::ADroppedKnobArrivesSquareAndLocked()
{
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Knob, 100, 100);

    auto const* const control = ControlAt(controller, 0);

    VERIFY_IS_TRUE(control->AspectLocked);
    VerifyNear(control->Width, control->Height);
}

void EditorControllerTests::ADroppedControlIsSelected()
{
    auto controller = LoadedController();

    auto const id = controller.AddControl(glass::ControlKind::Fader, 100, 100);

    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.Selection().size());
    VERIFY_IS_TRUE(controller.IsSelected(id));
}

void EditorControllerTests::ADroppedControlSnapsToTheGrid()
{
    auto controller = LoadedController();

    controller.SetGridSize(8.0);
    controller.AddControl(glass::ControlKind::Fader, 101, 103);

    VerifyNear(104.0, ControlAt(controller, 0)->X);
    VerifyNear(104.0, ControlAt(controller, 0)->Y);
}

void EditorControllerTests::TheKeyboardPathFindsAFreeSpot()
{
    // Somebody who cannot click the page still has to be able to put a control on it, and
    // landing every one of them on top of the last is not a usable answer.
    auto controller = LoadedController();

    for (int index = 0; index < 6; ++index)
    {
        VERIFY_IS_FALSE(controller.AddControlAtFreeSpot(glass::ControlKind::Pad).empty());
    }

    VERIFY_ARE_EQUAL(size_t{ 6 }, ControlCount(controller));

    for (size_t outer = 0; outer < 6; ++outer)
    {
        for (size_t inner = outer + 1; inner < 6; ++inner)
        {
            auto const* const a = ControlAt(controller, outer);
            auto const* const b = ControlAt(controller, inner);

            glass::EditRect const first{ a->X, a->Y, a->Width, a->Height };
            glass::EditRect const second{ b->X, b->Y, b->Width, b->Height };

            VERIFY_IS_FALSE(glass::Intersects(first, second));
        }
    }
}

void EditorControllerTests::TheKeyboardPathStaysOnThePage()
{
    auto controller = LoadedController();

    for (int index = 0; index < 12; ++index)
    {
        controller.AddControlAtFreeSpot(glass::ControlKind::Fader);
    }

    VERIFY_ARE_EQUAL(size_t{ 0 }, controller.ControlsOutsidePage().size());
}

void EditorControllerTests::APageWillNotOverflow()
{
    auto controller = LoadedController();

    for (size_t index = 0; index < glass::MaximumControlsPerPage; ++index)
    {
        VERIFY_IS_FALSE(controller.AddControl(glass::ControlKind::Lamp, 8.0, 8.0).empty());
    }

    VERIFY_IS_TRUE(controller.AddControl(glass::ControlKind::Lamp, 8.0, 8.0).empty());
}

void EditorControllerTests::DeleteRemovesOnlyTheSelection()
{
    auto controller = LoadedController();

    auto const first = controller.AddControl(glass::ControlKind::Fader, 100, 100);
    controller.AddControl(glass::ControlKind::Knob, 300, 100);

    controller.SelectOnly(first);
    VERIFY_IS_TRUE(controller.DeleteSelection());

    VERIFY_ARE_EQUAL(size_t{ 1 }, ControlCount(controller));
    VERIFY_IS_TRUE(ControlAt(controller, 0)->Kind == glass::ControlKind::Knob);
}

void EditorControllerTests::DuplicateOffsetsTheCopyAndSelectsIt()
{
    auto controller = LoadedController();

    auto const id = controller.AddControl(glass::ControlKind::Fader, 104, 104);

    controller.SelectOnly(id);
    VERIFY_IS_TRUE(controller.DuplicateSelection());

    VERIFY_ARE_EQUAL(size_t{ 2 }, ControlCount(controller));

    auto const* const copy = ControlAt(controller, 1);

    VERIFY_IS_GREATER_THAN(copy->X, 104.0);
    VERIFY_ARE_NOT_EQUAL(id, copy->Id);
    VERIFY_IS_TRUE(controller.IsSelected(copy->Id));
    VERIFY_IS_FALSE(controller.IsSelected(id));
}

// ---- finding a control ----

void EditorControllerTests::ControlIndexCountsEveryPageInOrder()
{
    // This is the index the binding engine and the surface both use, so the monitor rail's
    // filter is only right if it counts the same way: every page in order, not per page.
    auto controller = LoadedController();

    auto const first = controller.AddControl(glass::ControlKind::Fader, 104, 104);
    auto const second = controller.AddControl(glass::ControlKind::Knob, 304, 104);

    VERIFY_IS_TRUE(controller.AddPage(L"Page 2"));
    controller.SetPageIndex(1);

    auto const third = controller.AddControl(glass::ControlKind::Pad, 104, 104);

    VERIFY_ARE_EQUAL(0, controller.ControlIndexOf(first));
    VERIFY_ARE_EQUAL(1, controller.ControlIndexOf(second));
    VERIFY_ARE_EQUAL(2, controller.ControlIndexOf(third));
}

void EditorControllerTests::ControlIndexOfSomethingMissingIsMinusOne()
{
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 104, 104);

    VERIFY_ARE_EQUAL(-1, controller.ControlIndexOf(L"nothing-has-this-id"));
}

// ---- moving ----

void EditorControllerTests::DraggingMovesTheWholeSelectionTogether()
{
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    auto const second = PlaceExactly(controller, glass::ControlKind::Fader, 200, 100, 40, 180);

    controller.SelectOnly(first);
    controller.AddToSelection(second);
    controller.SetSnapSuspended(true);

    controller.BeginDrag();
    controller.UpdateDrag(50.0, 30.0);
    controller.EndDrag();

    VerifyNear(150.0, ControlAt(controller, 0)->X);
    VerifyNear(250.0, ControlAt(controller, 1)->X);
    VerifyNear(130.0, ControlAt(controller, 1)->Y);
}

void EditorControllerTests::DraggingSnapsTheLeadAndCarriesTheRest()
{
    // The whole selection follows one rectangle, so a bank keeps its own spacing rather than
    // every control snapping to a different guide.
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    auto const second = PlaceExactly(controller, glass::ControlKind::Fader, 153, 100, 40, 180);

    controller.SelectOnly(first);
    controller.AddToSelection(second);

    controller.BeginDrag();
    controller.UpdateDrag(3.0, 0.0);
    controller.EndDrag();

    auto const gap = ControlAt(controller, 1)->X - ControlAt(controller, 0)->X;

    VerifyNear(53.0, gap);
}

void EditorControllerTests::AStraightLineDragFollowsTheLongerWay()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);

    controller.SelectOnly(id);
    controller.SetSnapSuspended(true);

    controller.BeginDrag();

    // Mostly across, so it moves across only.
    controller.UpdateDrag(60.0, 20.0, true);

    VerifyNear(160.0, ControlAt(controller, 0)->X);
    VerifyNear(100.0, ControlAt(controller, 0)->Y);

    // The same drag turned downward. Measured from the start, down is now the longer way.
    controller.UpdateDrag(10.0, 70.0, true);

    VerifyNear(100.0, ControlAt(controller, 0)->X);
    VerifyNear(170.0, ControlAt(controller, 0)->Y);

    controller.EndDrag();
}

void EditorControllerTests::AStraightLineDragIsNotPulledOffItsLine()
{
    // Two faders a few pixels out of line. Dragging the first across, with guides on, would
    // pull its top onto the second's; held to a straight line, it must not move down at all.
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    PlaceExactly(controller, glass::ControlKind::Fader, 300, 103, 40, 180);

    controller.SelectOnly(first);

    controller.BeginDrag();
    auto const outcome = controller.UpdateDrag(120.0, 1.0, true);
    controller.EndDrag();

    VerifyNear(100.0, ControlAt(controller, 0)->Y);

    for (auto const& guide : outcome.Guides)
    {
        VERIFY_ARE_NOT_EQUAL(static_cast<int>(glass::GuideAxis::Horizontal), static_cast<int>(guide.Axis));
    }
}

void EditorControllerTests::ADragIsMeasuredFromWhereItStarted()
{
    // Accumulating each update onto the last would drift, and the drift shows up as a control
    // that ends somewhere other than under the pointer.
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);

    controller.SelectOnly(id);
    controller.SetSnapSuspended(true);
    controller.BeginDrag();

    for (int step = 1; step <= 100; ++step)
    {
        controller.UpdateDrag(step * 1.37, 0.0);
    }

    controller.EndDrag();

    VerifyNear(100.0 + 137.0, ControlAt(controller, 0)->X);
}

void EditorControllerTests::NudgingMovesBySomethingExact()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);

    controller.SelectOnly(id);

    VERIFY_IS_TRUE(controller.NudgeSelection(1.0, 0.0));
    VerifyNear(101.0, ControlAt(controller, 0)->X);

    VERIFY_IS_TRUE(controller.NudgeSelection(8.0, 0.0));
    VerifyNear(109.0, ControlAt(controller, 0)->X);
}

void EditorControllerTests::TypedBoundsAreNotSnapped()
{
    auto controller = LoadedController();

    auto const id = controller.AddControl(glass::ControlKind::Fader, 100, 100);

    VERIFY_IS_TRUE(controller.SetControlBounds(id, 103.0, 107.0, 41.0, 181.0));

    auto const* const control = ControlAt(controller, 0);

    VerifyNear(103.0, control->X);
    VerifyNear(107.0, control->Y);
    VerifyNear(41.0, control->Width);
    VerifyNear(181.0, control->Height);
}

void EditorControllerTests::AControlCanBeDraggedOffThePage()
{
    // Never clamped, because clamping is what makes a resize unrecoverable.
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);

    controller.SelectOnly(id);
    controller.SetSnapSuspended(true);

    controller.BeginDrag();
    controller.UpdateDrag(-400.0, 0.0);
    controller.EndDrag();

    VerifyNear(-300.0, ControlAt(controller, 0)->X);
    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.ControlsOutsidePage().size());
}

// ---- arranging ----

void EditorControllerTests::AlignLeftUsesTheLeftmostEdge()
{
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Pad, 200, 100, 56, 56);
    auto const second = PlaceExactly(controller, glass::ControlKind::Pad, 120, 200, 56, 56);
    auto const third = PlaceExactly(controller, glass::ControlKind::Pad, 300, 300, 56, 56);

    controller.SelectOnly(first);
    controller.AddToSelection(second);
    controller.AddToSelection(third);

    VERIFY_IS_TRUE(controller.AlignSelection(glass::AlignEdge::Left));

    VerifyNear(120.0, ControlAt(controller, 0)->X);
    VerifyNear(120.0, ControlAt(controller, 1)->X);
    VerifyNear(120.0, ControlAt(controller, 2)->X);
}

void EditorControllerTests::DistributeEqualizesTheGaps()
{
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Pad, 100, 100, 56, 56);
    PlaceExactly(controller, glass::ControlKind::Pad, 200, 100, 56, 56);
    PlaceExactly(controller, glass::ControlKind::Pad, 500, 100, 56, 56);

    controller.SelectAll();
    VERIFY_IS_TRUE(controller.DistributeSelection(glass::ArrangeAxis::Horizontal));

    auto const gaps = controller.SelectionGaps(glass::ArrangeAxis::Horizontal);

    VERIFY_ARE_EQUAL(size_t{ 2 }, gaps.size());
    VerifyNear(gaps[0], gaps[1]);
}

void EditorControllerTests::DistributeLeavesTheOutermostTwoAlone()
{
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Pad, 100, 100, 56, 56);
    PlaceExactly(controller, glass::ControlKind::Pad, 200, 100, 56, 56);
    PlaceExactly(controller, glass::ControlKind::Pad, 500, 100, 56, 56);

    controller.SelectAll();
    VERIFY_IS_TRUE(controller.DistributeSelection(glass::ArrangeAxis::Horizontal));

    VerifyNear(100.0, ControlAt(controller, 0)->X);
    VerifyNear(500.0, ControlAt(controller, 2)->X);
}

void EditorControllerTests::SettingAGapSpacesThemAll()
{
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Pad, 100, 100, 56, 56);
    PlaceExactly(controller, glass::ControlKind::Pad, 300, 100, 56, 56);
    PlaceExactly(controller, glass::ControlKind::Pad, 700, 100, 56, 56);

    controller.SelectAll();
    VERIFY_IS_TRUE(controller.SetSelectionGap(glass::ArrangeAxis::Horizontal, 16.0));

    VerifyNear(100.0, ControlAt(controller, 0)->X);
    VerifyNear(172.0, ControlAt(controller, 1)->X);
    VerifyNear(244.0, ControlAt(controller, 2)->X);
}

void EditorControllerTests::GapsAreMeasuredInPositionOrderNotSelectionOrder()
{
    auto controller = LoadedController();

    auto const left = PlaceExactly(controller, glass::ControlKind::Pad, 100, 100, 56, 56);
    auto const middle = PlaceExactly(controller, glass::ControlKind::Pad, 200, 100, 56, 56);
    auto const right = PlaceExactly(controller, glass::ControlKind::Pad, 300, 100, 56, 56);

    // Selected right to left on purpose.
    controller.SelectOnly(right);
    controller.AddToSelection(middle);
    controller.AddToSelection(left);

    auto const gaps = controller.SelectionGaps(glass::ArrangeAxis::Horizontal);

    VERIFY_ARE_EQUAL(size_t{ 2 }, gaps.size());
    VerifyNear(44.0, gaps[0]);
    VerifyNear(44.0, gaps[1]);
}

// ---- which control is drawn over which ----

namespace
{
    // Three pads in a known draw order, returned in that order.
    std::vector<std::wstring> ThreeInOrder(_Inout_ glass::EditorController& controller)
    {
        return
        {
            PlaceExactly(controller, glass::ControlKind::Pad, 100, 100, 56, 56),
            PlaceExactly(controller, glass::ControlKind::Pad, 200, 100, 56, 56),
            PlaceExactly(controller, glass::ControlKind::Pad, 300, 100, 56, 56),
        };
    }

    std::vector<std::wstring> DrawOrder(_In_ glass::EditorController const& controller)
    {
        std::vector<std::wstring> ids{};

        if (auto const* const page = controller.CurrentPage())
        {
            for (auto const& control : page->Controls)
            {
                ids.push_back(control.Id);
            }
        }

        return ids;
    }
}

void EditorControllerTests::BringToFrontPutsTheSelectionLastInDrawOrder()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    controller.SelectOnly(ids[0]);

    VERIFY_IS_TRUE(controller.ChangeZOrder(glass::ZOrderMove::ToFront));

    auto const order = DrawOrder(controller);

    VERIFY_ARE_EQUAL(ids[1], order[0]);
    VERIFY_ARE_EQUAL(ids[2], order[1]);
    VERIFY_ARE_EQUAL(ids[0], order[2]);
}

void EditorControllerTests::SendToBackPutsTheSelectionFirst()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    controller.SelectOnly(ids[2]);

    VERIFY_IS_TRUE(controller.ChangeZOrder(glass::ZOrderMove::ToBack));

    auto const order = DrawOrder(controller);

    VERIFY_ARE_EQUAL(ids[2], order[0]);
    VERIFY_ARE_EQUAL(ids[0], order[1]);
    VERIFY_ARE_EQUAL(ids[1], order[2]);
}

void EditorControllerTests::BringForwardMovesOneStepOnly()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    controller.SelectOnly(ids[0]);

    VERIFY_IS_TRUE(controller.ChangeZOrder(glass::ZOrderMove::Forward));

    auto const order = DrawOrder(controller);

    VERIFY_ARE_EQUAL(ids[1], order[0]);
    VERIFY_ARE_EQUAL(ids[0], order[1]);
    VERIFY_ARE_EQUAL(ids[2], order[2]);
}

void EditorControllerTests::SendBackwardMovesOneStepOnly()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    controller.SelectOnly(ids[2]);

    VERIFY_IS_TRUE(controller.ChangeZOrder(glass::ZOrderMove::Backward));

    auto const order = DrawOrder(controller);

    VERIFY_ARE_EQUAL(ids[0], order[0]);
    VERIFY_ARE_EQUAL(ids[2], order[1]);
    VERIFY_ARE_EQUAL(ids[1], order[2]);
}

void EditorControllerTests::ABlockOfSelectedControlsMovesTogether()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    // The two at the bottom, brought forward as one. Without the block rule the lower of the
    // two would swap past the upper and the pair would come apart.
    controller.SelectOnly(ids[0]);
    controller.AddToSelection(ids[1]);

    VERIFY_IS_TRUE(controller.ChangeZOrder(glass::ZOrderMove::Forward));

    auto const order = DrawOrder(controller);

    VERIFY_ARE_EQUAL(ids[2], order[0]);
    VERIFY_ARE_EQUAL(ids[0], order[1]);
    VERIFY_ARE_EQUAL(ids[1], order[2]);
}

void EditorControllerTests::AControlAlreadyAtTheFrontDoesNotMove()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    controller.SelectOnly(ids[2]);

    VERIFY_IS_FALSE(controller.ChangeZOrder(glass::ZOrderMove::Forward));
    VERIFY_IS_FALSE(controller.ChangeZOrder(glass::ZOrderMove::ToFront));

    auto const order = DrawOrder(controller);

    VERIFY_ARE_EQUAL(ids[0], order[0]);
    VERIFY_ARE_EQUAL(ids[1], order[1]);
    VERIFY_ARE_EQUAL(ids[2], order[2]);
}

void EditorControllerTests::ChangingTheOrderLeavesTheKeyboardOrderAlone()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    // Which control is drawn over which and the order a screen reader walks are two different
    // ideas, and moving one must never quietly move the other.
    std::vector<int32_t> before{};

    for (auto const& id : ids)
    {
        before.push_back(controller.Document().FindControl(id)->KeyboardOrder);
    }

    controller.SelectOnly(ids[0]);

    VERIFY_IS_TRUE(controller.ChangeZOrder(glass::ZOrderMove::ToFront));

    for (size_t index = 0; index < ids.size(); ++index)
    {
        VERIFY_ARE_EQUAL(before[index], controller.Document().FindControl(ids[index])->KeyboardOrder);
    }
}

void EditorControllerTests::ChangingTheOrderCanBeTakenBack()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    controller.SelectOnly(ids[0]);

    VERIFY_IS_TRUE(controller.ChangeZOrder(glass::ZOrderMove::ToFront));
    VERIFY_IS_TRUE(controller.Undo());

    auto const order = DrawOrder(controller);

    VERIFY_ARE_EQUAL(ids[0], order[0]);
    VERIFY_ARE_EQUAL(ids[1], order[1]);
    VERIFY_ARE_EQUAL(ids[2], order[2]);
}

// ---- grouping panels ----

void EditorControllerTests::ADroppedGroupPanelGoesToTheBack()
{
    auto controller = LoadedController();
    auto const ids = ThreeInOrder(controller);

    auto const panel = controller.AddControl(glass::ControlKind::Panel, 80, 80);

    VERIFY_IS_FALSE(panel.empty());

    // Dropped on top of the controls it is meant to frame, it would hide them, and nobody drops
    // one meaning that.
    auto const order = DrawOrder(controller);

    VERIFY_ARE_EQUAL(panel, order[0]);
    VERIFY_ARE_EQUAL(ids[0], order[1]);
}

void EditorControllerTests::AGroupPanelSendsNothing()
{
    auto controller = LoadedController();

    auto const panel = controller.AddControl(glass::ControlKind::Panel, 80, 80);

    VERIFY_ARE_EQUAL(size_t{ 0 }, controller.Document().FindControl(panel)->Messages.size());
    VERIFY_IS_FALSE(glass::SendsAnything(glass::ControlKind::Panel));
    VERIFY_IS_FALSE(glass::IsInteractive(glass::ControlKind::Panel));
}

void EditorControllerTests::AGroupPanelArrivesAsAnOutline()
{
    auto controller = LoadedController();

    auto const panel = controller.AddControl(glass::ControlKind::Panel, 80, 80);
    auto const* const control = controller.Document().FindControl(panel);

    VERIFY_IS_TRUE(control->Style == glass::ControlStyleOverride::Outline);
    VERIFY_IS_TRUE(control->LabelPlaced == glass::LabelPlacementOverride::Inside);
}

// ---- repeat ----

void EditorControllerTests::RepeatBuildsABank()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    controller.SelectOnly(id);

    glass::RepeatOptions options{};
    options.Copies = 7;
    options.Direction = glass::RepeatDirection::Right;
    options.Gap = 16.0;

    VERIFY_IS_TRUE(controller.RepeatSelection(options));

    // The strip is 40 wide, so the stride is 56.
    VERIFY_ARE_EQUAL(size_t{ 8 }, ControlCount(controller));
    VerifyNear(156.0, ControlAt(controller, 1)->X);
    VerifyNear(492.0, ControlAt(controller, 7)->X);
}

void EditorControllerTests::RepeatStepsTheChannel()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    controller.SelectOnly(id);

    glass::RepeatOptions options{};
    options.Copies = 7;
    options.Field = glass::RepeatField::Channel;
    options.Step = 1;

    VERIFY_IS_TRUE(controller.RepeatSelection(options));

    for (size_t index = 0; index < 8; ++index)
    {
        VERIFY_ARE_EQUAL(static_cast<int32_t>(index), ControlAt(controller, index)->Messages[0].ChannelIndex);
    }
}

void EditorControllerTests::RepeatWrapsTheChannelAtSixteen()
{
    // A bank that runs off the end of the range and silently stops stepping is worse than one
    // that comes round again.
    std::vector<glass::Control> source{};

    glass::Control control{};
    control.Id = L"source";

    glass::ControlMessage message{};
    message.ChannelIndex = 14;
    control.Messages.push_back(message);

    source.push_back(control);

    glass::RepeatOptions options{};
    options.Copies = 3;
    options.Field = glass::RepeatField::Channel;
    options.Step = 1;

    auto const plan = glass::BuildRepeat(source, options);

    VERIFY_ARE_EQUAL(size_t{ 3 }, plan.Copies.size());
    VERIFY_ARE_EQUAL(15, plan.Copies[0].Messages[0].ChannelIndex);
    VERIFY_ARE_EQUAL(0, plan.Copies[1].Messages[0].ChannelIndex);
    VERIFY_ARE_EQUAL(1, plan.Copies[2].Messages[0].ChannelIndex);
}

void EditorControllerTests::RepeatStepsTheControllerNumber()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    controller.SelectOnly(id);

    auto const first = ControlAt(controller, 0)->Messages[0].Number;

    glass::RepeatOptions options{};
    options.Copies = 3;
    options.Field = glass::RepeatField::ControllerNumber;
    options.Step = 1;

    VERIFY_IS_TRUE(controller.RepeatSelection(options));

    VERIFY_ARE_EQUAL(first + 1, ControlAt(controller, 1)->Messages[0].Number);
    VERIFY_ARE_EQUAL(first + 3, ControlAt(controller, 3)->Messages[0].Number);
}

void EditorControllerTests::RepeatStepsTheFeedbackToMatch()
{
    // Without this, copy two sends on channel two and listens on channel one.
    std::vector<glass::Control> source{};

    glass::Control control{};
    control.Id = L"source";

    glass::ControlMessage message{};
    message.ChannelIndex = 0;
    control.Messages.push_back(message);

    control.Feedback.Enabled = true;
    control.Feedback.ChannelIndex = 0;

    source.push_back(control);

    glass::RepeatOptions options{};
    options.Copies = 2;
    options.Field = glass::RepeatField::Channel;
    options.Step = 1;

    auto const plan = glass::BuildRepeat(source, options);

    VERIFY_ARE_EQUAL(1, plan.Copies[0].Feedback.ChannelIndex);
    VERIFY_ARE_EQUAL(2, plan.Copies[1].Feedback.ChannelIndex);
}

void EditorControllerTests::RepeatLeavesAllGroupsAlone()
{
    // Stepping "all groups" would silently bind the copy to one.
    std::vector<glass::Control> source{};

    glass::Control control{};
    control.Id = L"source";

    glass::ControlMessage message{};
    message.GroupIndex = glass::AllGroups;
    control.Messages.push_back(message);

    source.push_back(control);

    glass::RepeatOptions options{};
    options.Copies = 2;
    options.Field = glass::RepeatField::Group;
    options.Step = 1;

    auto const plan = glass::BuildRepeat(source, options);

    VERIFY_ARE_EQUAL(glass::AllGroups, plan.Copies[0].Messages[0].GroupIndex);
    VERIFY_ARE_EQUAL(glass::AllGroups, plan.Copies[1].Messages[0].GroupIndex);
}

void EditorControllerTests::RepeatFillsTheLabelPattern()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"Ch 1" }, glass::FormatRepeatLabel(L"Ch {n}", 1));
    VERIFY_ARE_EQUAL(std::wstring{ L"Ch 12" }, glass::FormatRepeatLabel(L"Ch {n}", 12));
    VERIFY_ARE_EQUAL(std::wstring{ L"3 of 3" }, glass::FormatRepeatLabel(L"{n} of {n}", 3));
    VERIFY_ARE_EQUAL(std::wstring{ L"Same" }, glass::FormatRepeatLabel(L"Same", 4));

    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    controller.SelectOnly(id);

    glass::RepeatOptions options{};
    options.Copies = 7;
    options.LabelPattern = L"Ch {n}";

    VERIFY_IS_TRUE(controller.RepeatSelection(options));

    VERIFY_ARE_EQUAL(std::wstring{ L"Ch 1" }, ControlAt(controller, 0)->Label);
    VERIFY_ARE_EQUAL(std::wstring{ L"Ch 8" }, ControlAt(controller, 7)->Label);
}

void EditorControllerTests::RepeatMovesAWholeStripAsAUnit()
{
    // Repeating a channel strip repeats the strip, not each control separately.
    auto controller = LoadedController();

    auto const fader = PlaceExactly(controller, glass::ControlKind::Fader, 100, 200, 40, 180);
    auto const knob = PlaceExactly(controller, glass::ControlKind::Knob, 100, 100, 56, 56);

    controller.SelectOnly(fader);
    controller.AddToSelection(knob);

    glass::RepeatOptions options{};
    options.Copies = 1;
    options.Direction = glass::RepeatDirection::Right;
    options.Gap = 16.0;

    VERIFY_IS_TRUE(controller.RepeatSelection(options));

    VERIFY_ARE_EQUAL(size_t{ 4 }, ControlCount(controller));

    // The strip is 56 wide at its widest, so the whole thing moves 72.
    VerifyNear(172.0, ControlAt(controller, 2)->X);
    VerifyNear(172.0, ControlAt(controller, 3)->X);
    VerifyNear(200.0, ControlAt(controller, 2)->Y);
    VerifyNear(100.0, ControlAt(controller, 3)->Y);
}

void EditorControllerTests::RepeatGivesEveryCopyItsOwnIdentity()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    controller.SelectOnly(id);

    glass::RepeatOptions options{};
    options.Copies = 7;

    VERIFY_IS_TRUE(controller.RepeatSelection(options));

    std::vector<std::wstring> ids{};
    std::vector<int32_t> orders{};

    for (size_t index = 0; index < 8; ++index)
    {
        ids.push_back(ControlAt(controller, index)->Id);
        orders.push_back(ControlAt(controller, index)->KeyboardOrder);
    }

    for (size_t outer = 0; outer < ids.size(); ++outer)
    {
        for (size_t inner = outer + 1; inner < ids.size(); ++inner)
        {
            VERIFY_ARE_NOT_EQUAL(ids[outer], ids[inner]);
            VERIFY_ARE_NOT_EQUAL(orders[outer], orders[inner]);
        }
    }
}

void EditorControllerTests::RepeatIsOneUndoEntry()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);
    controller.SelectOnly(id);

    auto const depth = controller.UndoDepth();

    glass::RepeatOptions options{};
    options.Copies = 7;

    VERIFY_IS_TRUE(controller.RepeatSelection(options));
    VERIFY_ARE_EQUAL(depth + 1, controller.UndoDepth());

    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_ARE_EQUAL(size_t{ 1 }, ControlCount(controller));
}

// ---- the label's own box ----

void EditorControllerTests::SettingALabelBoxMakesThePlacementCustom()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);

    VERIFY_IS_TRUE(controller.SetControlLabelBox(id, -20.0, 185.0, 80.0, 32.0));

    auto const* const control = ControlAt(controller, 0);

    // The box and the placement say the same thing, so they move together.
    VERIFY_IS_TRUE(control->LabelPlaced == glass::LabelPlacementOverride::Custom);
    VERIFY_IS_TRUE(control->LabelLook.HasBox());
    VERIFY_ARE_EQUAL(-20.0, control->LabelLook.BoxX);
    VERIFY_ARE_EQUAL(80.0, control->LabelLook.BoxWidth);
}

void EditorControllerTests::ALabelBoxIsBounded()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Knob, 100, 100, 56, 56);

    // Dragging a corner past its opposite must not leave a box of nothing, and nothing may ask
    // for a text block the size of a wall.
    VERIFY_IS_TRUE(controller.SetControlLabelBox(id, 0.0, 0.0, -50.0, 1.0e9));

    auto const& look = ControlAt(controller, 0)->LabelLook;

    VERIFY_ARE_EQUAL(glass::MinimumLabelBoxSize, look.BoxWidth);
    VERIFY_ARE_EQUAL(glass::MaximumLabelBoxExtent, look.BoxHeight);

    // Something that is not a number never reaches the document.
    VERIFY_IS_FALSE(controller.SetControlLabelBox(
        id, std::numeric_limits<double>::quiet_NaN(), 0.0, 40.0, 20.0));

    VERIFY_ARE_EQUAL(glass::MinimumLabelBoxSize, ControlAt(controller, 0)->LabelLook.BoxWidth);
}

void EditorControllerTests::ClearingALabelBoxGoesBackToTheTheme()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);

    VERIFY_IS_TRUE(controller.SetControlLabelBox(id, -20.0, 185.0, 80.0, 32.0));
    VERIFY_IS_TRUE(controller.ClearControlLabelBox(id));

    auto const* const control = ControlAt(controller, 0);

    VERIFY_IS_FALSE(control->LabelLook.HasBox());
    VERIFY_IS_TRUE(control->LabelPlaced == glass::LabelPlacementOverride::UseTheme);

    // Clearing what is already clear changes nothing, so it does not spend an undo entry.
    VERIFY_IS_FALSE(controller.ClearControlLabelBox(id));
}

void EditorControllerTests::ChangingTheFontLeavesTheLabelBoxAlone()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);

    VERIFY_IS_TRUE(controller.SetControlLabelBox(id, -20.0, 185.0, 80.0, 32.0));

    glass::LabelStyle style{};
    style.FontSize = 18.0;
    style.Italic = true;

    VERIFY_IS_TRUE(controller.SetControlLabelStyle(id, style));

    auto const& look = ControlAt(controller, 0)->LabelLook;

    // The box has its own gesture. A change of font must never move the label somebody dragged.
    VERIFY_ARE_EQUAL(18.0, look.FontSize);
    VERIFY_IS_TRUE(look.Italic);
    VERIFY_ARE_EQUAL(-20.0, look.BoxX);
    VERIFY_ARE_EQUAL(80.0, look.BoxWidth);
}

void EditorControllerTests::ALabelDragIsOneUndoEntry()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 180);

    auto const depth = controller.UndoDepth();

    // A drag is a run of pointer moves. Each one is a real edit, and together they are one
    // thing somebody did, so Ctrl+Z takes the whole drag back rather than one pixel of it.
    for (double width = 60.0; width <= 100.0; width += 10.0)
    {
        VERIFY_IS_TRUE(controller.SetControlLabelBox(id, -20.0, 185.0, width, 32.0));
    }

    controller.EndCoalescing();

    VERIFY_ARE_EQUAL(depth + 1, controller.UndoDepth());

    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_FALSE(ControlAt(controller, 0)->LabelLook.HasBox());
}

// ---- keyboard order ----

void EditorControllerTests::KeyboardOrderShiftsRatherThanDuplicating()
{
    auto controller = LoadedController();

    std::vector<std::wstring> ids{};

    for (int index = 0; index < 4; ++index)
    {
        ids.push_back(controller.AddControl(glass::ControlKind::Pad, 64.0 * index, 64.0));
    }

    // Move the fourth control to the front.
    VERIFY_IS_TRUE(controller.SetKeyboardOrder(ids[3], 1));

    std::vector<int32_t> orders{};

    for (size_t index = 0; index < 4; ++index)
    {
        orders.push_back(ControlAt(controller, index)->KeyboardOrder);
    }

    VERIFY_ARE_EQUAL(2, orders[0]);
    VERIFY_ARE_EQUAL(3, orders[1]);
    VERIFY_ARE_EQUAL(4, orders[2]);
    VERIFY_ARE_EQUAL(1, orders[3]);
}

void EditorControllerTests::RenumberClosesTheGaps()
{
    auto controller = LoadedController();

    auto const first = controller.AddControl(glass::ControlKind::Pad, 0, 0);
    auto const second = controller.AddControl(glass::ControlKind::Pad, 64, 0);
    auto const third = controller.AddControl(glass::ControlKind::Pad, 128, 0);

    controller.SelectOnly(second);
    VERIFY_IS_TRUE(controller.DeleteSelection());

    VERIFY_IS_TRUE(controller.RenumberKeyboardOrder());

    VERIFY_ARE_EQUAL(1, ControlAt(controller, 0)->KeyboardOrder);
    VERIFY_ARE_EQUAL(2, ControlAt(controller, 1)->KeyboardOrder);

    VERIFY_ARE_EQUAL(first, ControlAt(controller, 0)->Id);
    VERIFY_ARE_EQUAL(third, ControlAt(controller, 1)->Id);
}

void EditorControllerTests::SortByPositionReadsInRows()
{
    // Comparing Y outright would put a fader whose top edge is two pixels higher than its
    // neighbor's before the whole row beside it, which is not how anyone reads a mixer.
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Pad, 300, 102, 56, 56);
    PlaceExactly(controller, glass::ControlKind::Pad, 100, 100, 56, 56);
    PlaceExactly(controller, glass::ControlKind::Pad, 100, 400, 56, 56);

    VERIFY_IS_TRUE(controller.SortKeyboardOrderByPosition());

    VERIFY_ARE_EQUAL(2, ControlAt(controller, 0)->KeyboardOrder);
    VERIFY_ARE_EQUAL(1, ControlAt(controller, 1)->KeyboardOrder);
    VERIFY_ARE_EQUAL(3, ControlAt(controller, 2)->KeyboardOrder);
}

// ---- the page ----

void EditorControllerTests::GrowingThePageKeepsEverythingInside()
{
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Fader, 1200, 700, 40, 80);

    glass::PageResizeRequest request{};
    request.NewWidth = 1920;
    request.NewHeight = 1080;
    request.Anchor = glass::CanvasAnchor::Center;

    VERIFY_IS_TRUE(controller.ResizePage(request));

    VERIFY_ARE_EQUAL(size_t{ 0 }, controller.ControlsOutsidePage().size());
    VerifyNear(1520.0, ControlAt(controller, 0)->X);
}

void EditorControllerTests::ShrinkingNeverClampsAControl()
{
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Fader, 1200, 700, 40, 80);

    glass::PageResizeRequest request{};
    request.NewWidth = 1024;
    request.NewHeight = 768;
    request.Anchor = glass::CanvasAnchor::TopLeft;

    VERIFY_IS_TRUE(controller.ResizePage(request));

    VerifyNear(1200.0, ControlAt(controller, 0)->X);
    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.ControlsOutsidePage().size());
}

void EditorControllerTests::AResizeCanBeTakenBackExactly()
{
    // The exit criterion for the virtual canvas.
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Fader, 1200, 700, 40, 80);

    glass::PageResizeRequest grow{};
    grow.NewWidth = 1920;
    grow.NewHeight = 1080;
    grow.Anchor = glass::CanvasAnchor::Center;

    VERIFY_IS_TRUE(controller.ResizePage(grow));
    VERIFY_IS_TRUE(controller.Undo());

    VERIFY_ARE_EQUAL(1280, controller.Document().PageWidth);
    VerifyNear(1200.0, ControlAt(controller, 0)->X);
    VerifyNear(700.0, ControlAt(controller, 0)->Y);
}

void EditorControllerTests::TheOffPageCountIsKnownBeforeCommitting()
{
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Fader, 1200, 100, 40, 80);
    PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 80);

    glass::PageResizeRequest request{};
    request.NewWidth = 1024;
    request.NewHeight = 768;
    request.Anchor = glass::CanvasAnchor::TopLeft;

    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.CountOutsideAfterResize(request));
    VERIFY_ARE_EQUAL(1280, controller.Document().PageWidth);
}

void EditorControllerTests::SelectingOffPageControlsFindsThem()
{
    auto controller = LoadedController();

    PlaceExactly(controller, glass::ControlKind::Fader, 1400, 100, 40, 80);
    PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 80);

    controller.SelectOutsidePage();

    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.Selection().size());
    VerifyNear(1400.0, controller.SelectedControls()[0]->X);
}

void EditorControllerTests::APageAlwaysHasOnePage()
{
    auto controller = LoadedController();

    VERIFY_IS_FALSE(controller.RemovePage(0));

    VERIFY_IS_TRUE(controller.AddPage(L"Second"));
    VERIFY_IS_TRUE(controller.RemovePage(1));
    VERIFY_IS_FALSE(controller.RemovePage(0));
}

// ---- devices ----

void EditorControllerTests::RenamingADeviceRewritesEveryControlThatUsedIt()
{
    // Destinations are named rather than wired, which is the whole point of the table.
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);
    controller.AddControl(glass::ControlKind::Fader, 200, 100);

    VERIFY_IS_TRUE(controller.RenameDevice(L"Synth", L"Iridium"));

    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium" }, ControlAt(controller, 0)->Messages[0].DeviceName);
    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium" }, ControlAt(controller, 1)->Messages[0].DeviceName);
    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium" }, controller.Document().Devices[0].Name);
}

void EditorControllerTests::RemovingADeviceLeavesTheNamesAlone()
{
    // A destination that no longer resolves is reported at run time. Silently rewriting two
    // hundred controls would be worse.
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);

    VERIFY_IS_TRUE(controller.RemoveDevice(L"Synth"));

    VERIFY_ARE_EQUAL(size_t{ 0 }, controller.Document().Devices.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Synth" }, ControlAt(controller, 0)->Messages[0].DeviceName);
}

void EditorControllerTests::TwoDevicesCannotShareAName()
{
    auto controller = LoadedController();

    glass::DeviceEntry duplicate{};
    duplicate.Name = L"Synth";

    VERIFY_IS_FALSE(controller.AddDevice(duplicate));

    glass::DeviceEntry second{};
    second.Name = L"DAW";

    VERIFY_IS_TRUE(controller.AddDevice(second));
    VERIFY_IS_FALSE(controller.RenameDevice(L"DAW", L"Synth"));
}

// ---- saved state ----

void EditorControllerTests::AFreshDocumentIsNotDirty()
{
    auto controller = LoadedController();

    VERIFY_IS_FALSE(controller.IsDirty());
}

void EditorControllerTests::AnEditMakesItDirty()
{
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);

    VERIFY_IS_TRUE(controller.IsDirty());
}

void EditorControllerTests::SavingClearsIt()
{
    auto controller = LoadedController();

    controller.AddControl(glass::ControlKind::Fader, 100, 100);
    controller.MarkSaved();

    VERIFY_IS_FALSE(controller.IsDirty());

    // And taking an edit back is itself a change that has to be saved.
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_TRUE(controller.IsDirty());
}

// ---- the clipboard ----

void EditorControllerTests::APasteLandsBesideTheOriginalsWithNewIds()
{
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Knob, 96, 96, 64, 64);
    auto const second = PlaceExactly(controller, glass::ControlKind::Fader, 200, 96, 40, 180);

    controller.SelectOnly(first);
    controller.AddToSelection(second);

    auto const text = controller.CopySelection();

    VERIFY_IS_FALSE(text.empty());
    VERIFY_IS_TRUE(controller.PasteControls(text));
    VERIFY_ARE_EQUAL(size_t{ 4 }, ControlCount(controller));

    // The copies are what is selected now, under ids of their own.
    VERIFY_ARE_EQUAL(size_t{ 2 }, controller.Selection().size());
    VERIFY_IS_FALSE(controller.IsSelected(first));
    VERIFY_IS_FALSE(controller.IsSelected(second));

    // One step down and across, so they are not hidden under the controls they came from.
    auto const* const copy = ControlAt(controller, 2);

    VERIFY_IS_TRUE(copy->X > 96.0);
    VERIFY_IS_TRUE(copy->Y > 96.0);
    VerifyNear(copy->X - 96.0, copy->Y - 96.0);

    // A second paste goes a step further rather than on top of the first.
    VERIFY_IS_TRUE(controller.PasteControls(text));
    VERIFY_IS_TRUE(ControlAt(controller, 4)->X > copy->X);
}

void EditorControllerTests::ACutThenPasteLandsWhereItWas()
{
    auto controller = LoadedController();

    auto const id = PlaceExactly(controller, glass::ControlKind::Knob, 96, 96, 64, 64);

    controller.SelectOnly(id);

    auto const text = controller.CutSelection();

    VERIFY_IS_FALSE(text.empty());
    VERIFY_ARE_EQUAL(size_t{ 0 }, ControlCount(controller));

    VERIFY_IS_TRUE(controller.PasteControls(text));
    VERIFY_ARE_EQUAL(size_t{ 1 }, ControlCount(controller));
    VerifyNear(96.0, ControlAt(controller, 0)->X);
    VerifyNear(96.0, ControlAt(controller, 0)->Y);
}

void EditorControllerTests::APasteIntoAnotherLayoutBringsItsDevice()
{
    auto source = LoadedController();

    auto const id = PlaceExactly(source, glass::ControlKind::Fader, 96, 96, 40, 180);

    source.SelectOnly(id);

    auto const text = source.CopySelection();

    auto document = EmptyDocument();
    document.Devices.clear();

    glass::EditorController target{};
    target.Load(std::move(document));

    VERIFY_IS_TRUE(target.PasteControls(text));

    // The fader sends to "Synth", so the layout it lands in learns what "Synth" is.
    VERIFY_IS_NOT_NULL(target.Document().FindDevice(L"Synth"));
    VERIFY_ARE_EQUAL(size_t{ 1 }, target.Document().Devices.size());
}

void EditorControllerTests::AReferenceBetweenCopiesFollowsThem()
{
    auto controller = LoadedController();

    auto const knob = PlaceExactly(controller, glass::ControlKind::Knob, 96, 96, 64, 64);
    auto const clock = PlaceExactly(controller, glass::ControlKind::BeatClock, 200, 96, 64, 64);

    auto spec = controller.Document().FindControl(clock)->Clock;
    spec.TempoControlId = knob;

    VERIFY_IS_TRUE(controller.SetControlClock(clock, spec));

    controller.SelectOnly(knob);
    controller.AddToSelection(clock);

    VERIFY_IS_TRUE(controller.PasteControls(controller.CopySelection()));

    // The pasted clock follows the pasted knob, not the one it was copied beside.
    auto const* const pastedKnob = ControlAt(controller, 2);
    auto const* const pastedClock = ControlAt(controller, 3);

    VERIFY_ARE_EQUAL(pastedKnob->Id, pastedClock->Clock.TempoControlId);
}

void EditorControllerTests::PastedTextBecomesOneTextControl()
{
    auto controller = LoadedController();

    VERIFY_IS_TRUE(controller.PasteText(L"  Filter\r\n\tcutoff  ", 640.0, 400.0));
    VERIFY_ARE_EQUAL(size_t{ 1 }, ControlCount(controller));

    auto const* const text = ControlAt(controller, 0);

    VERIFY_ARE_EQUAL(static_cast<int>(glass::ControlKind::Label), static_cast<int>(text->Kind));
    VERIFY_ARE_EQUAL(std::wstring{ L"Filter cutoff" }, text->Label);
    VERIFY_IS_TRUE(controller.IsSelected(text->Id));

    // Roughly where it was asked for.
    VERIFY_IS_TRUE(std::abs((text->X + text->Width / 2.0) - 640.0) <= 16.0);

    // Nothing to show is not a control.
    VERIFY_IS_FALSE(controller.PasteText(L" \r\n ", 640.0, 400.0));
}

void EditorControllerTests::APasteOfSomethingElseDoesNothing()
{
    auto controller = LoadedController();

    VERIFY_IS_FALSE(controller.PasteControls(L""));
    VERIFY_IS_FALSE(controller.PasteControls(L"this is not a layout"));
    VERIFY_IS_FALSE(controller.PasteControls(L"{\"name\":\"x\"}"));
    VERIFY_ARE_EQUAL(size_t{ 0 }, ControlCount(controller));
    VERIFY_IS_FALSE(controller.CanUndo());
}

// ---- groups and several at once ----

void EditorControllerTests::GroupingMakesOneClickPickTheWholeGroup()
{
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Knob, 96, 96, 64, 64);
    auto const second = PlaceExactly(controller, glass::ControlKind::Fader, 200, 96, 40, 180);
    auto const loose = PlaceExactly(controller, glass::ControlKind::Pad, 400, 96, 64, 64);

    controller.SelectOnly(first);
    controller.AddToSelection(second);

    VERIFY_IS_TRUE(controller.GroupSelection());
    VERIFY_IS_TRUE(controller.SelectionIsOneGroup());

    // Grouping a group again changes nothing.
    VERIFY_IS_FALSE(controller.GroupSelection());

    controller.SelectGroupOf(loose);
    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.Selection().size());

    controller.SelectGroupOf(second);
    VERIFY_ARE_EQUAL(size_t{ 2 }, controller.Selection().size());
    VERIFY_IS_TRUE(controller.IsSelected(first));

    // Adding the loose pad with Shift takes it alone; taking the group back out takes both.
    controller.ToggleGroupOf(loose);
    VERIFY_ARE_EQUAL(size_t{ 3 }, controller.Selection().size());

    controller.ToggleGroupOf(first);
    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.Selection().size());
    VERIFY_IS_TRUE(controller.IsSelected(loose));

    // A rubber band touching one member brings the rest along.
    controller.SelectInRectangle({ 90, 90, 20, 20 }, false);
    controller.ExpandSelectionToGroups();
    VERIFY_ARE_EQUAL(size_t{ 2 }, controller.Selection().size());
}

void EditorControllerTests::UngroupingLetsThemGoTheirOwnWay()
{
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Knob, 96, 96, 64, 64);
    auto const second = PlaceExactly(controller, glass::ControlKind::Fader, 200, 96, 40, 180);

    controller.SelectOnly(first);
    controller.AddToSelection(second);
    VERIFY_IS_TRUE(controller.GroupSelection());

    VERIFY_IS_TRUE(controller.UngroupSelection());
    VERIFY_IS_FALSE(controller.SelectionHasGroup());
    VERIFY_IS_FALSE(controller.UngroupSelection());

    controller.SelectGroupOf(first);
    VERIFY_ARE_EQUAL(size_t{ 1 }, controller.Selection().size());

    // And it is one undo step back to a group.
    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_FALSE(ControlAt(controller, 0)->GroupId.empty());
    VERIFY_ARE_EQUAL(ControlAt(controller, 0)->GroupId, ControlAt(controller, 1)->GroupId);
}

void EditorControllerTests::ACopyOfAGroupIsAGroupOfItsOwn()
{
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Knob, 96, 96, 64, 64);
    auto const second = PlaceExactly(controller, glass::ControlKind::Fader, 200, 96, 40, 180);

    controller.SelectOnly(first);
    controller.AddToSelection(second);
    VERIFY_IS_TRUE(controller.GroupSelection());

    VERIFY_IS_TRUE(controller.DuplicateSelection());

    auto const original = ControlAt(controller, 0)->GroupId;
    auto const copy = ControlAt(controller, 2)->GroupId;

    VERIFY_IS_FALSE(copy.empty());
    VERIFY_ARE_NOT_EQUAL(original, copy);
    VERIFY_ARE_EQUAL(copy, ControlAt(controller, 3)->GroupId);

    // One member on its own is copied as a control on its own, not a group of one.
    controller.SelectOnly(first);
    VERIFY_IS_TRUE(controller.PasteControls(controller.CopySelection()));
    VERIFY_IS_TRUE(ControlAt(controller, 4)->GroupId.empty());
}

void EditorControllerTests::AnEditBatchIsOneUndoStep()
{
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Knob, 96, 96, 64, 64);
    auto const second = PlaceExactly(controller, glass::ControlKind::Knob, 200, 96, 64, 64);

    auto const depth = controller.UndoDepth();

    {
        glass::EditBatch batch{ controller };

        VERIFY_IS_TRUE(controller.SetControlStyle(first, glass::ControlStyleOverride::Outline));
        VERIFY_IS_TRUE(controller.SetControlStyle(second, glass::ControlStyleOverride::Outline));
    }

    VERIFY_ARE_EQUAL(depth + 1, controller.UndoDepth());

    // The next edit is its own step, not part of the batch.
    VERIFY_IS_TRUE(controller.SetControlStyle(first, glass::ControlStyleOverride::Solid));
    VERIFY_ARE_EQUAL(depth + 2, controller.UndoDepth());

    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_TRUE(controller.Undo());

    VERIFY_ARE_EQUAL(static_cast<int>(glass::ControlStyleOverride::UseTheme), static_cast<int>(ControlAt(controller, 0)->Style));
    VERIFY_ARE_EQUAL(static_cast<int>(glass::ControlStyleOverride::UseTheme), static_cast<int>(ControlAt(controller, 1)->Style));
}

void EditorControllerTests::ScalingSeveralKeepsTheirPlacesInTheBox()
{
    auto controller = LoadedController();

    auto const fader = PlaceExactly(controller, glass::ControlKind::Fader, 100, 100, 40, 200);
    auto const knob = PlaceExactly(controller, glass::ControlKind::Knob, 240, 100, 60, 60);

    controller.SelectOnly(fader);
    controller.AddToSelection(knob);

    auto const before = controller.SelectionBounds();

    VerifyNear(100.0, before.X);
    VerifyNear(200.0, before.Width);
    VerifyNear(200.0, before.Height);

    // Twice as wide, the same height, from the same corner.
    VERIFY_IS_TRUE(controller.SetSelectionBounds(100.0, 100.0, 400.0, 200.0));

    auto const* const stretched = controller.Document().FindControl(fader);
    auto const* const round = controller.Document().FindControl(knob);

    VerifyNear(100.0, stretched->X);
    VerifyNear(80.0, stretched->Width);
    VerifyNear(200.0, stretched->Height);

    // A knob keeps its shape, and stays centered where its middle went.
    VerifyNear(round->Width, round->Height);
    VerifyNear(100.0 + ((270.0 - 100.0) * 2.0), round->X + (round->Width / 2.0));

    // Moving only moves.
    auto const now = controller.SelectionBounds();

    VERIFY_IS_TRUE(controller.SetSelectionBounds(now.X + 50.0, now.Y, now.Width, now.Height));
    VerifyNear(150.0, controller.Document().FindControl(fader)->X);
    VerifyNear(80.0, controller.Document().FindControl(fader)->Width);
}

void EditorControllerTests::AGroupSurvivesSavingAndLoading()
{
    auto controller = LoadedController();

    auto const first = PlaceExactly(controller, glass::ControlKind::Knob, 96, 96, 64, 64);
    auto const second = PlaceExactly(controller, glass::ControlKind::Fader, 200, 96, 40, 180);

    controller.SelectOnly(first);
    controller.AddToSelection(second);
    VERIFY_IS_TRUE(controller.GroupSelection());

    auto const read = glass::ReadLayoutFromJson(glass::WriteLayoutToJson(controller.Document()));

    VERIFY_IS_TRUE(read.Succeeded);

    auto const& controls = read.Document.Pages.front().Controls;

    VERIFY_ARE_EQUAL(size_t{ 2 }, controls.size());
    VERIFY_IS_FALSE(controls[0].GroupId.empty());
    VERIFY_ARE_EQUAL(controls[0].GroupId, controls[1].GroupId);
}

void EditorControllerTests::RemovingASwitchPositionRenumbersItsRows()
{
    auto controller = LoadedController();

    auto const id = controller.AddControl(glass::ControlKind::Switch, 100, 100);

    // Three positions, three rows. A fourth brings a row of its own.
    VERIFY_IS_TRUE(controller.AddSwitchPosition(id));

    auto const* switched = controller.Document().FindControl(id);

    VERIFY_ARE_EQUAL(size_t{ 4 }, switched->Switch.Positions.size());
    VERIFY_ARE_EQUAL(size_t{ 4 }, switched->Messages.size());
    VERIFY_ARE_EQUAL(3, switched->Messages[3].Position);

    // Taking the second away takes its row, and the rows after it move down one.
    VERIFY_IS_TRUE(controller.RemoveSwitchPosition(id, 1));

    switched = controller.Document().FindControl(id);

    VERIFY_ARE_EQUAL(size_t{ 3 }, switched->Switch.Positions.size());
    VERIFY_ARE_EQUAL(size_t{ 3 }, switched->Messages.size());
    VERIFY_ARE_EQUAL(0, switched->Messages[0].Position);
    VERIFY_ARE_EQUAL(1, switched->Messages[1].Position);
    VERIFY_ARE_EQUAL(2, switched->Messages[2].Position);

    // Never fewer than two.
    VERIFY_IS_TRUE(controller.RemoveSwitchPosition(id, 0));
    VERIFY_IS_FALSE(controller.RemoveSwitchPosition(id, 0));

    VERIFY_IS_TRUE(controller.SetSwitchPositionName(id, 1, L"Wide"));
    VERIFY_ARE_EQUAL(std::wstring{ L"Wide" }, controller.Document().FindControl(id)->Switch.Positions[1]);
}
