// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class LearnCaptureTests : public WEX::TestClass<LearnCaptureTests>
{
public:

    BEGIN_TEST_CLASS(LearnCaptureTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- reading a message back into a binding ----

    TEST_METHOD(LearnsAMidi1ControlChange);
    TEST_METHOD(LearnsAMidi2ControlChange);
    TEST_METHOD(LearnsANote);
    TEST_METHOD(LearnsPitchBend);
    TEST_METHOD(LearnsARegisteredController);
    TEST_METHOD(TakesTheGroupAndTheChannelToo);
    TEST_METHOD(IgnoresWhatCannotBeBound);

    // ---- what not to take ----

    TEST_METHOD(ANoteOffDoesNotArmAnything);
    TEST_METHOD(PitchBendAtCenterDoesNotArmAnything);

    // ---- writing it back ----

    TEST_METHOD(EveryFieldCanBeRefused);
    TEST_METHOD(LockingTheDeviceTakesOnlyTheNumber);
    TEST_METHOD(LearningFeedbackTurnsItOn);
    TEST_METHOD(OneKnobHeldIsOneBinding);
};
