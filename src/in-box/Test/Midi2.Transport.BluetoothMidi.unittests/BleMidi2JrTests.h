// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Unit tests for BLE MIDI 2.0 Jitter Reduction (JR) Clock and JR Timestamp handling in the UMP
// Characteristic. The rules are provisional, from M2-104-UM section 7.2.2 and BLE MIDI 2.0
// Working Group discussion, so these tests pin today's behavior and will move with the spec.
class BleMidi2JrTests
    : public WEX::TestClass<BleMidi2JrTests>
{
public:

    BEGIN_TEST_CLASS(BleMidi2JrTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // message helpers
    TEST_METHOD(TestJrMessagesAreRecognized);
    TEST_METHOD(TestStreamConfigurationNotificationIsParsed);
    TEST_METHOD(TestLocalTimeConvertsToJrTime);

    // incoming
    TEST_METHOD(TestDecoderRemovesJrMessages);
    TEST_METHOD(TestDecoderUsesArrivalTimeWithoutAClock);
    TEST_METHOD(TestDecoderPlacesMessagesByJrTimestamp);
    TEST_METHOD(TestDecoderSharesATimestampAcrossSimultaneousMessages);
    TEST_METHOD(TestDecoderNeverReturnsAFutureTimestamp);
    TEST_METHOD(TestDecoderFollowsTheClockAcrossAWrap);
    TEST_METHOD(TestDecoderFallsBackToArrivalWhenTheClockStops);
    TEST_METHOD(TestDecoderFallsBackToArrivalWhenTheClockGoesQuiet);
    TEST_METHOD(TestDecoderCarriesAnUnusedTimestampIntoTheNextPayload);
    TEST_METHOD(TestDecoderDoesNotCarryAUsedTimestamp);
    TEST_METHOD(TestDecoderResetForgetsTheClock);

    // outgoing
    TEST_METHOD(TestBuilderWithoutJrPacksMessagesUnchanged);
    TEST_METHOD(TestBuilderDropsApplicationJrMessages);
    TEST_METHOD(TestBuilderSharesOneTimestampWithinATick);
    TEST_METHOD(TestBuilderTimestampsEachMessageInADifferentTick);
    TEST_METHOD(TestBuilderKeepsRoomForTheClock);
    TEST_METHOD(TestBuilderSendsTheClockSeparatelyWhenItCannotFit);
    TEST_METHOD(TestClockOnlyPacket);
};
