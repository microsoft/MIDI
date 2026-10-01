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
    TEST_METHOD(AStraightLineDragFollowsTheLongerWay);
    TEST_METHOD(AStraightLineDragIsNotPulledOffItsLine);
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
    TEST_METHOD(AGroupIsSpacedAsOneBlock);
    TEST_METHOD(DistributeKeepsEachGroupInOnePiece);
    TEST_METHOD(AlignMovesAGroupAsOneBlock);
    TEST_METHOD(TheMembersOfOneGroupAreLinedUpOneByOne);
    TEST_METHOD(ATypedGapGoesToTheControlsItWasTypedFor);
    TEST_METHOD(AnArrangementThatMovesNothingIsNotAnEdit);

    // ---- which control is drawn over which ----

    TEST_METHOD(BringToFrontPutsTheSelectionLastInDrawOrder);
    TEST_METHOD(SendToBackPutsTheSelectionFirst);
    TEST_METHOD(BringForwardMovesOneStepOnly);
    TEST_METHOD(SendBackwardMovesOneStepOnly);
    TEST_METHOD(ABlockOfSelectedControlsMovesTogether);
    TEST_METHOD(AControlAlreadyAtTheFrontDoesNotMove);
    TEST_METHOD(ChangingTheOrderLeavesTheKeyboardOrderAlone);
    TEST_METHOD(ChangingTheOrderCanBeTakenBack);

    // ---- grouping panels ----

    TEST_METHOD(ADroppedGroupPanelGoesToTheBack);
    TEST_METHOD(AGroupPanelSendsNothing);
    TEST_METHOD(AGroupPanelArrivesInTheThemesStyle);

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

    // ---- the label's own box ----

    TEST_METHOD(SettingALabelBoxMakesThePlacementCustom);
    TEST_METHOD(ALabelBoxIsBounded);
    TEST_METHOD(ClearingALabelBoxGoesBackToTheTheme);
    TEST_METHOD(ChangingTheFontLeavesTheLabelBoxAlone);
    TEST_METHOD(ALabelDragIsOneUndoEntry);

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

    // ---- the clipboard ----

    TEST_METHOD(APasteLandsBesideTheOriginalsWithNewIds);
    TEST_METHOD(ACutThenPasteLandsWhereItWas);
    TEST_METHOD(APasteIntoAnotherLayoutBringsItsDevice);
    TEST_METHOD(AReferenceBetweenCopiesFollowsThem);
    TEST_METHOD(PastedTextBecomesOneTextControl);
    TEST_METHOD(APasteOfSomethingElseDoesNothing);

    // ---- groups and several at once ----

    TEST_METHOD(GroupingMakesOneClickPickTheWholeGroup);
    TEST_METHOD(UngroupingLetsThemGoTheirOwnWay);
    TEST_METHOD(ACopyOfAGroupIsAGroupOfItsOwn);
    TEST_METHOD(AnEditBatchIsOneUndoStep);
    TEST_METHOD(ScalingSeveralKeepsTheirPlacesInTheBox);
    TEST_METHOD(AGroupSurvivesSavingAndLoading);
    TEST_METHOD(RemovingASwitchPositionRenumbersItsRows);

    // ---- the outline ----

    TEST_METHOD(TheOutlineListsAGroupUnderItsHeading);
    TEST_METHOD(AGroupOfOneIsListedAsAControl);
    TEST_METHOD(AGroupsHeadingPicksEveryMember);

    // ---- what a group is called ----

    TEST_METHOD(AGroupCanBeNamed);
    TEST_METHOD(UngroupingOrDeletingForgetsTheName);
    TEST_METHOD(AddingToANamedGroupKeepsItsName);
    TEST_METHOD(ACopyOfANamedGroupIsNumbered);
    TEST_METHOD(ARepeatNumbersItsGroups);
    TEST_METHOD(MovingAPagesControlsTakesTheirNames);

    // ---- where several controls send and listen ----

    TEST_METHOD(SeveralControlsCanBeSentSomewhereElseAtOnce);
    TEST_METHOD(ADestinationLeavesRowsThatGoNowhereAlone);
    TEST_METHOD(ADestinationMustBeInTheDeviceTable);
    TEST_METHOD(OnlyTheControlsThatListenAreMoved);

    // ---- page tabs, pan controls and the window ----

    TEST_METHOD(ANewPageTabGoesToThePageItIsOn);
    TEST_METHOD(APanControlStartsInTheMiddle);
    TEST_METHOD(TheWindowSettingsCanBeUndone);
    TEST_METHOD(APageCanBeAsSmallAsAToolbar);
    TEST_METHOD(ThePaletteOffersOnlyWhatIsBuilt);

    // ---- the background picture and sequences ----

    TEST_METHOD(ThePictureItsFitAndItsOpacityAreOneStep);
    TEST_METHOD(DeletingASequenceLeavesTheRowsThatPlayedIt);

    // ---- locking ----

    TEST_METHOD(ALockedControlCannotBePickedOnThePage);
    TEST_METHOD(ALockedControlStaysWhereItIs);
    TEST_METHOD(LockingIsOneStepAndTravelsInTheFile);
};
