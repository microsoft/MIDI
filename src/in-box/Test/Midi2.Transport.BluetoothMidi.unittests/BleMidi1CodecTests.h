// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Unit tests for the BLE MIDI 1.0 Characteristic packet format, as defined by MMA/AMEI RP-052
// and restated in BLE-MIDI 2.0 (TSB #274) section 4.
//
// This is the only part of the transport that can be tested without a real BLE peripheral, and
// it is also the part most likely to break silently, so the packet-level expectations are
// spelled out byte by byte rather than only round-tripped.
class BleMidi1CodecTests
    : public WEX::TestClass<BleMidi1CodecTests>
{
public:

    BEGIN_TEST_CLASS(BleMidi1CodecTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // packet framing
    TEST_METHOD(TestEmptyPayloadIsAccepted);
    TEST_METHOD(TestPacketWithoutHeaderBitIsRejected);
    TEST_METHOD(TestSingleFullMessageIsDecoded);
    TEST_METHOD(TestTruncatedMessageDoesNotOverrun);
    TEST_METHOD(TestTrailingTimestampByteIsIgnored);

    // timestamps
    TEST_METHOD(TestTimestampSpacingIsRelativeToFirstMessage);
    TEST_METHOD(TestTimestampLowWrapIncrementsTimestampHigh);

    // running status
    TEST_METHOD(TestRunningStatusWithTimestampIsExpanded);
    TEST_METHOD(TestRunningStatusWithoutTimestampInheritsTimestamp);
    TEST_METHOD(TestRunningStatusIsCanceledByEndOfPacket);
    TEST_METHOD(TestSystemCommonCancelsRunningStatus);
    TEST_METHOD(TestSystemRealTimeDoesNotCancelRunningStatus);

    // system exclusive
    TEST_METHOD(TestSysExInSinglePacketIsDecoded);
    TEST_METHOD(TestSysExSpanningPacketsIsDecoded);
    TEST_METHOD(TestSystemRealTimeInsideSysExDoesNotEndIt);

    // encoding
    TEST_METHOD(TestBuilderWritesHeaderAndTimestampBytes);
    TEST_METHOD(TestBuilderPacksMultipleMessagesIntoOnePacket);
    TEST_METHOD(TestBuilderNeverSplitsANonSysExMessage);
    TEST_METHOD(TestBuilderStartsNewPacketWhenTimestampHighChanges);
    TEST_METHOD(TestBuilderSplitsSysExAcrossPacketsWithoutTimestampBytes);
    TEST_METHOD(TestBuilderGivesSysExEndByteItsOwnTimestampByte);
    TEST_METHOD(TestBuilderGivesRealTimeInsideSysExItsOwnTimestampByte);

    // the two halves have to agree
    TEST_METHOD(TestRoundTripOfChannelMessages);
    TEST_METHOD(TestRoundTripOfSysExAcrossPackets);

    // Timestamp correlation across packets
    TEST_METHOD(TestCorrelatorKeepsSpacingWithinAPacket);
    TEST_METHOD(TestCorrelatorIsMonotonicAcrossPackets);
    TEST_METHOD(TestCorrelatorHandlesSenderClockWrap);
    TEST_METHOD(TestCorrelatorNeverReturnsAFutureTimestamp);
    TEST_METHOD(TestCorrelatorIgnoresImplausibleBackwardsJump);
    TEST_METHOD(TestCorrelatorResetRebuildsMapping);
    TEST_METHOD(TestDecoderReportsSenderTimestamp);
};
