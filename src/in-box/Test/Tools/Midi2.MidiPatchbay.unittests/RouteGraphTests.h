// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class RouteGraphTests : public WEX::TestClass<RouteGraphTests>
{
public:

    BEGIN_TEST_CLASS(RouteGraphTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AChainRoutesTheWayItReads);
    TEST_METHOD(EachPathGetsItsOwnCopy);
    TEST_METHOD(OnlyTheChosenSourceGroupGoesIn);
    TEST_METHOD(NothingIsBuiltForAnAbsentDestination);
    TEST_METHOD(ALoopBetweenBlocksStopsOnlyThatPatch);
    TEST_METHOD(AThrottleIsOneQueue);
    TEST_METHOD(MutedLinksAndBypassedBlocks);
    TEST_METHOD(APatchTooDeepDoesNotRoute);
    TEST_METHOD(TheSignatureFollowsWhatRoutes);
    TEST_METHOD(AConvertedPatchRoutesLikeVersion1);
    TEST_METHOD(AGeneratorStartsATreeOfItsOwn);
    TEST_METHOD(AGeneratorRunsOnlyWhenItLeadsSomewhere);
    TEST_METHOD(NothingGoesIntoAClockOrTimeCode);
    TEST_METHOD(AnLfoFollowsTheClockConnectedToIt);
    TEST_METHOD(AClockStepOrADividerCanDriveAnLfo);

    // The examples in docs/kb/midi-patchbay-patches-for-agents.md, word for word. If one of these
    // fails, the guide is telling agents to write something that doesn't do what it says.
    TEST_METHOD(TheAgentGuideSplitExampleRoutes);
    TEST_METHOD(TheAgentGuideMaskExampleRoutes);
    TEST_METHOD(TheAgentGuideClockExampleRoutes);
    TEST_METHOD(TheAgentGuideLfoExampleReads);
};
