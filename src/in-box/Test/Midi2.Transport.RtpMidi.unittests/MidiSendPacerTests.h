// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The send speed limit shared by the Network MIDI 2.0 and RTP-MIDI transports: the pacer, the
// MIDI 1.0 size of each UMP message, and the automatic slow down. A clock of the test's own, so
// every run is the same.
class MidiSendPacerTests
    : public WEX::TestClass<MidiSendPacerTests>
{
public:

    BEGIN_TEST_CLASS(MidiSendPacerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestNoLimitNeverWaits);
    TEST_METHOD(TestQuietConnectionIsNeverDelayed);
    TEST_METHOD(TestChordGoesAtOnce);
    TEST_METHOD(TestBurstGoesAtWireSpeed);
    TEST_METHOD(TestBurstGoesFasterAtAHigherLimit);
    TEST_METHOD(TestMessageLargerThanTheAllowanceGoesAtOnce);
    TEST_METHOD(TestResetForgetsWhatWasSent);
    TEST_METHOD(TestAnythingAboveTheFastestLimitIsNoLimit);
    TEST_METHOD(TestMidi1SizeOfEachMessageType);

    TEST_METHOD(TestAutomaticSlowDownStepsDownToWireSpeed);
    TEST_METHOD(TestAutomaticSlowDownSpeedsBackUp);
    TEST_METHOD(TestAutomaticSlowDownWaitsLongerAfterARaiseFails);
    TEST_METHOD(TestAutomaticSlowDownFromNoLimit);
    TEST_METHOD(TestAutomaticSlowDownOffChangesNothing);
};
