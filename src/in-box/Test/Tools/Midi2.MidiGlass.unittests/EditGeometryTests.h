// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class EditGeometryTests : public WEX::TestClass<EditGeometryTests>
{
public:

    BEGIN_TEST_CLASS(EditGeometryTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- the grid ----

    TEST_METHOD(SnapRoundsToTheNearestCell);
    TEST_METHOD(SnapWithNoGridLeavesTheValueAlone);

    // ---- magnetic guides ----

    TEST_METHOD(AGuideBeatsTheGrid);
    TEST_METHOD(LeftEdgesLineUp);
    TEST_METHOD(CentersLineUp);
    TEST_METHOD(APageMarginPulls);
    TEST_METHOD(SomethingFarAwayDoesNotPull);
    TEST_METHOD(TheNearestCandidateWins);
    TEST_METHOD(AGuideReachesBothRectangles);
    TEST_METHOD(SuspendingSnapLeavesThePositionExact);
    TEST_METHOD(BothAxesCanSnapAtOnce);

    // ---- handles ----

    TEST_METHOD(DraggingTheRightEdgeLeavesTheLeftAlone);
    TEST_METHOD(DraggingTheLeftEdgeMovesTheOrigin);
    TEST_METHOD(AControlCannotBeDraggedToNothing);
    TEST_METHOD(ACornerCanHoldTheAspectRatio);
    TEST_METHOD(AnEdgeHandleCannotHoldTheAspectRatio);
    TEST_METHOD(ResizingSnapsOnlyTheEdgeBeingDragged);

    // ---- off the page ----

    TEST_METHOD(AControlHalfOffThePageCountsAsOutside);
    TEST_METHOD(AControlOnTheEdgeIsInside);

    // ---- changing the page size ----

    TEST_METHOD(GrowingWithATopLeftAnchorMovesNothing);
    TEST_METHOD(GrowingWithACenterAnchorCentersEverything);
    TEST_METHOD(GrowingWithABottomRightAnchorTakesAllTheSpace);
    TEST_METHOD(ScalingToFitUsesOneFactorForBothAxes);
    TEST_METHOD(ScalingToFitCentersWhatIsLeftOver);
    TEST_METHOD(ShrinkingAndLeavingCountsWhatFallsOutside);
    TEST_METHOD(ScalingToFitLeavesNothingOutside);
    TEST_METHOD(GrowThenShrinkBackPutsEverythingWhereItStarted);

    // ---- the work area ----

    TEST_METHOD(TheWorkAreaSurroundsThePage);
    TEST_METHOD(TheWorkAreaGrowsToHoldAnOffPageControl);
};
