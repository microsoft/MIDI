// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiFeedbackDetector.h"
#include "MidiFeedbackGuard.h"

#include <WexTestClass.h>

class FeedbackDetectorTests
    : public WEX::TestClass<FeedbackDetectorTests>
{
public:

    BEGIN_TEST_CLASS(FeedbackDetectorTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // loops that must be caught
    TEST_METHOD(TestFastNoteLoopTrips);
    TEST_METHOD(TestSaturatedStormTrips);
    TEST_METHOD(TestClockLoopTrips);
    TEST_METHOD(TestSystemExclusiveLoopTrips);
    TEST_METHOD(TestSlowDawStyleLoopTrips);

    // traffic that must never trip and must never lose or reorder a message
    TEST_METHOD(TestQuietTrafficIsNeverHeld);
    TEST_METHOD(TestBenchmarkPatternIsLossless);
    TEST_METHOD(TestBatchedBenchmarkPatternIsLossless);
    TEST_METHOD(TestRepeatingAutomationIsLossless);
    TEST_METHOD(TestDistinctSystemExclusiveDumpIsLossless);
    TEST_METHOD(TestSenderThatStopsDuringPauseDoesNotTrip);
    TEST_METHOD(TestFullHoldQueueMeansNotALoop);

    // behavior around the edges
    TEST_METHOD(TestBackoffDoublesAfterEachFalseAlarm);
    TEST_METHOD(TestResetAfterTripDeliversAgain);
    TEST_METHOD(TestEndTestReleasesWithoutBackoff);
    TEST_METHOD(TestSlowLoopIsBelowDetection);
    TEST_METHOD(TestMalformedBufferIsSafe);

    // the thread-safe wrapper, in real time
    TEST_METHOD(TestGuardKeepsEveryMessageInOrder);
    TEST_METHOD(TestGuardTripsOnLiveLoop);
    TEST_METHOD(TestGuardResetDiscardsAndResumes);
};
