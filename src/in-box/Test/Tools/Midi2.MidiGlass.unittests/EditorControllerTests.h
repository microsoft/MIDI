// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class EditorControllerTests : public WEX::TestClass<EditorControllerTests>
{
public:

    BEGIN_TEST_CLASS(EditorControllerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- undo ----

    TEST_METHOD(NothingToUndoOnAFreshDocument);
    TEST_METHOD(AnEditCanBeTakenBack);
    TEST_METHOD(RedoPutsItBack);
    TEST_METHOD(ANewEditClearsTheRedoBranch);
    TEST_METHOD(ADragIsOneEntry);
    TEST_METHOD(TwoDragsAreTwoEntries);
    TEST_METHOD(WritingBackAnUnchangedValueIsNotAnEdit);
    TEST_METHOD(ShowingAControlDoesNotThrowAwayTheRedoBranch);
    TEST_METHOD(UndoNamesTheEdit);
    TEST_METHOD(TheStackIsBounded);
    TEST_METHOD(UndoRestoresEveryControlADeleteTookOut);

    // ---- placing ----

    TEST_METHOD(ADroppedControlSendsSomething);
    TEST_METHOD(ADroppedDisplayControlSendsNothing);
    TEST_METHOD(EightDroppedFadersDoNotAllUseControllerOne);
    TEST_METHOD(ADroppedKnobArrivesSquareAndLocked);
    TEST_METHOD(ADroppedControlIsSelected);
    TEST_METHOD(ADroppedControlSnapsToTheGrid);
    TEST_METHOD(TheKeyboardPathFindsAFreeSpot);
    TEST_METHOD(TheKeyboardPathStaysOnThePage);
    TEST_METHOD(APageWillNotOverflow);
    TEST_METHOD(DeleteRemovesOnlyTheSelection);
    TEST_METHOD(DuplicateOffsetsTheCopyAndSelectsIt);

    // ---- finding a control ----

    TEST_METHOD(ControlIndexCountsEveryPageInOrder);
    TEST_METHOD(ControlIndexOfSomethingMissingIsMinusOne);

    // ---- moving ----

    TEST_METHOD(DraggingMovesTheWholeSelectionTogether);
    TEST_METHOD(DraggingSnapsTheLeadAndCarriesTheRest);
    TEST_METHOD(ADragIsMeasuredFromWhereItStarted);
    TEST_METHOD(NudgingMovesBySomethingExact);
    TEST_METHOD(TypedBoundsAreNotSnapped);
    TEST_METHOD(AControlCanBeDraggedOffThePage);

    // ---- arranging ----

    TEST_METHOD(AlignLeftUsesTheLeftmostEdge);
    TEST_METHOD(DistributeEqualizesTheGaps);
    TEST_METHOD(DistributeLeavesTheOutermostTwoAlone);
    TEST_METHOD(SettingAGapSpacesThemAll);
    TEST_METHOD(GapsAreMeasuredInPositionOrderNotSelectionOrder);

    // ---- repeat ----

    TEST_METHOD(RepeatBuildsABank);
    TEST_METHOD(RepeatStepsTheChannel);
    TEST_METHOD(RepeatWrapsTheChannelAtSixteen);
    TEST_METHOD(RepeatStepsTheControllerNumber);
    TEST_METHOD(RepeatStepsTheFeedbackToMatch);
    TEST_METHOD(RepeatLeavesAllGroupsAlone);
    TEST_METHOD(RepeatFillsTheLabelPattern);
    TEST_METHOD(RepeatMovesAWholeStripAsAUnit);
    TEST_METHOD(RepeatGivesEveryCopyItsOwnIdentity);
    TEST_METHOD(RepeatIsOneUndoEntry);

    // ---- keyboard order ----

    TEST_METHOD(KeyboardOrderShiftsRatherThanDuplicating);
    TEST_METHOD(RenumberClosesTheGaps);
    TEST_METHOD(SortByPositionReadsInRows);

    // ---- the page ----

    TEST_METHOD(GrowingThePageKeepsEverythingInside);
    TEST_METHOD(ShrinkingNeverClampsAControl);
    TEST_METHOD(AResizeCanBeTakenBackExactly);
    TEST_METHOD(TheOffPageCountIsKnownBeforeCommitting);
    TEST_METHOD(SelectingOffPageControlsFindsThem);
    TEST_METHOD(APageAlwaysHasOnePage);

    // ---- devices ----

    TEST_METHOD(RenamingADeviceRewritesEveryControlThatUsedIt);
    TEST_METHOD(RemovingADeviceLeavesTheNamesAlone);
    TEST_METHOD(TwoDevicesCannotShareAName);

    // ---- saved state ----

    TEST_METHOD(AFreshDocumentIsNotDirty);
    TEST_METHOD(AnEditMakesItDirty);
    TEST_METHOD(SavingClearsIt);
};
