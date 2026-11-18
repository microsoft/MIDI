// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Hostile and racing input for the Launchpad X code: random messages, broken SysEx, sequences far
// bigger or smaller than the pads, and the device's thread racing the app's. Each run is seeded,
// so a failure repeats.
class LaunchpadXHardeningTests : public WEX::TestClass<LaunchpadXHardeningTests>
{
public:
    BEGIN_TEST_CLASS(LaunchpadXHardeningTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(RandomMessagesNeverAskForSomethingOutOfRange);
    TEST_METHOD(BrokenSysExNeverGrowsPastItsLimitOrBlocksTheNextMessage);
    TEST_METHOD(EmptyAndHugeSequencesStillPaintEveryLed);
    TEST_METHOD(ModeChangesRacingPadsNeverLeaveANoteOn);
    TEST_METHOD(StopRacingPadsNeverLeavesANoteOn);
};
