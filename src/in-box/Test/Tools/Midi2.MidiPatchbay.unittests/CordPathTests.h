// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Where connection lines run on the canvas: the usual curve when nothing is in the way, and
// around the endpoints and steps that are, never through them.
class CordPathTests : public WEX::TestClass<CordPathTests>
{
public:

    BEGIN_TEST_CLASS(CordPathTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AClearLineKeepsTheUsualCurve);
    TEST_METHOD(ALineBackToANodeBelowGoesOverItsTop);
    TEST_METHOD(ALineBackToItsOwnNodeGoesAroundIt);
    TEST_METHOD(ALineStaysOnTheCanvas);
    TEST_METHOD(SomethingInTheWayIsGoneAround);
    TEST_METHOD(NoLineCrossesANode);
    TEST_METHOD(LinesAreSmoothAndMeetTheirPointsLevel);
    TEST_METHOD(LinesSharingAWayAreSpreadApart);
    TEST_METHOD(LabelsSitOnTheirLinesClearOfEachOther);
    TEST_METHOD(TheSameLayoutGivesTheSameLines);
    TEST_METHOD(ABigPatchIsQuick);
    TEST_METHOD(NonsenseDoesNotStopIt);
};
