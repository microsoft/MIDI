// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class GridButtonTests : public WEX::TestClass<GridButtonTests>
{
public:

    BEGIN_TEST_CLASS(GridButtonTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- rows and columns ----

    TEST_METHOD(RowsLayOutASetGridFromTheBottomLeft);
    TEST_METHOD(ColumnsTurnTheGridOnItsSide);
    TEST_METHOD(PadsStretchToFillTheControl);
    TEST_METHOD(AGridNeverHasMoreThan1024Pads);
    TEST_METHOD(HexagonsInColumnsLieFlatSideUp);
    TEST_METHOD(RowsSurviveARoundTripAndFlowStaysOutOfTheFile);

    // ---- what a grid button does ----

    TEST_METHOD(OctaveRowAndShiftButtonsMoveTheGrid);
    TEST_METHOD(AChangeThatLeavesTheNoteRangeIsNotMade);
    TEST_METHOD(InKeyOnlyNeedsAKey);
    TEST_METHOD(NextRootAndNextScaleGoRoundAgain);
    TEST_METHOD(NextRowOffsetStepsThroughThePresets);
    TEST_METHOD(ResetPutsTheGridBack);

    // ---- targets ----

    TEST_METHOD(AButtonChangesOnlyTheGridsItTargets);
    TEST_METHOD(APickerWithTargetsChangesAndShowsOnlyItsGrids);
    TEST_METHOD(InKeyRowOffsetAndOctavePickers);
    TEST_METHOD(AReadoutReadsTheGridItTargets);

    // ---- sending and the file ----

    TEST_METHOD(APadGridRowSendsNothingAndRunsAsAPlan);
    TEST_METHOD(PadGridRowsSurviveARoundTrip);

    // ---- the editor ----

    TEST_METHOD(AddingGridButtonsPutsWiredControlsUnderTheGrid);
    TEST_METHOD(GridButtonsGoBelowWhatIsAlreadyThere);
    TEST_METHOD(PickersAddedForASecondGridTargetOnlyThatGrid);
    TEST_METHOD(TheEditorKeepsRowsWithinTheCap);

    // ---- note names typed into a note box ----

    TEST_METHOD(ANoteNameReadsAsItsNumber);
    TEST_METHOD(ANoteThatIsNotOneReadsAsNothing);
};
