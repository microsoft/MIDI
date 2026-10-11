// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Automation lanes and tempo map editing: lane values, how lanes render, the file, and the edits.
class AutomationTests : public WEX::TestClass<AutomationTests>
{
public:

    BEGIN_TEST_CLASS(AutomationTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(LaneValuesFollowThePoints);
    TEST_METHOD(ARampSendsOnlyWhatTheDestinationCanTell);
    TEST_METHOD(APieceAtATimeMatchesTheWhole);
    TEST_METHOD(AClipLaneWinsInsideItsPlacement);
    TEST_METHOD(AClipLaneStartsOverEachPass);
    TEST_METHOD(StartValuesSendWhereTheLaneIs);
    TEST_METHOD(TheStepFollowsTheIntervalAndTheFastestTempo);
    TEST_METHOD(LaneMessagesSpeakEachTarget);
    TEST_METHOD(LanesRoundTripThroughTheFile);
    TEST_METHOD(DamagedLanesAreClampedOrSkipped);
    TEST_METHOD(LanePointEdits);
    TEST_METHOD(LaneEditsUndo);
    TEST_METHOD(TempoEdits);
    TEST_METHOD(MeterEditsKeepLaterBars);
    TEST_METHOD(LaneRowsFollowTheirTrack);
};
