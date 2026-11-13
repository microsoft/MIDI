// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Unit tests for incoming smoothing: recovering the sender's clock from timestamped packets and
// choosing when to release each message. Most run against a simulated link, with connection
// events, Windows stack delay and a sender clock which can run fast or slow, so the results are
// measured jitter and latency rather than single values.
class BleIncomingSmoothingTests
    : public WEX::TestClass<BleIncomingSmoothingTests>
{
public:

    BEGIN_TEST_CLASS(BleIncomingSmoothingTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // clock estimator and delay tracker
    TEST_METHOD(TestClockEstimatorFindsTheLowerEdge);
    TEST_METHOD(TestClockEstimatorMeasuresSkew);
    TEST_METHOD(TestClockEstimatorIgnoresSkewOverAShortSpan);
    TEST_METHOD(TestClockEstimatorLimitsSkew);
    TEST_METHOD(TestDelayTrackerIgnoresARareOutlier);
    TEST_METHOD(TestDelayTrackerForgetsOldDelays);
    TEST_METHOD(TestDelayTrackerMeasuresAgainstTheCurrentClock);

    // smoother
    TEST_METHOD(TestSmootherReleasesOnArrivalWhileWarmingUp);
    TEST_METHOD(TestSmootherReleasesOnArrivalForAnUnusableClock);
    TEST_METHOD(TestSmootherRemovesConnectionIntervalJitter);
    TEST_METHOD(TestSmootherKeepsSpacingWithinAPacket);
    TEST_METHOD(TestSmootherFollowsASlowSenderClock);
    TEST_METHOD(TestSmootherFollowsAFastSenderClock);
    TEST_METHOD(TestSmootherDelaySettlesNearTheConnectionInterval);
    TEST_METHOD(TestSmootherDelayShrinksTowardWhatTheLinkNeeds);
    TEST_METHOD(TestSmootherGrowsTheDelayWhenMessagesArriveLate);
    TEST_METHOD(TestSmootherNeverReleasesEarlyOrOutOfOrder);
    TEST_METHOD(TestSmootherNeverHoldsLongerThanTheMaximum);
    TEST_METHOD(TestSmootherResetClockKeepsTheDelay);

    // BLE MIDI 1.0 correlator support
    TEST_METHOD(TestCorrelatorReportsContinuousSenderTime);
    TEST_METHOD(TestCorrelatorClockGenerationChangesOnResynchronize);
    TEST_METHOD(TestSmootherWithTheBleMidi1Correlator);
};
