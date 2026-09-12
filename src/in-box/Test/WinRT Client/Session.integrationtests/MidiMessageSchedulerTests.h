// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiMessageSchedulerTests
    : public WEX::TestClass<MidiMessageSchedulerTests>
{
public:

    BEGIN_TEST_CLASS(MidiMessageSchedulerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    //TEST_CLASS_SETUP(ClassSetup);
    //TEST_CLASS_CLEANUP(ClassCleanup);

    //TEST_METHOD_SETUP(TestSetup);
    //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestScheduledMessagesOrder);
    TEST_METHOD(TestScheduledMessagesTimingSmall);
    TEST_METHOD(TestScheduledMessagesTimingLarge);
    TEST_METHOD(TestScheduledMessagesTimingShortLead);

    // Reports which input group each output group comes back on, for a multi-port device wired
    // OUT to IN where the sockets are unlabelled. Same endpoint variable as the measurement below,
    // optionally bounded by MIDI_RTT_TEST_MAX_GROUP.
    TEST_METHOD(DiscoverLoopbackGroupMapping);

    // Measures the real round trip to a device wired OUT to IN. Only runs when
    // MIDI_RTT_TEST_ENDPOINT_ID names an endpoint, so it is inert in a normal test pass.
    TEST_METHOD(MeasureDeviceRoundTripLatency);

    void TestScheduledMessagesTiming(
        _In_ uint16_t const messageCount,
        _In_ uint32_t const scheduledTimeStampOffsetMS);

private:

};
