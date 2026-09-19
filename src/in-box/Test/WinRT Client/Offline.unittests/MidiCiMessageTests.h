// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiCiMessageTests
    : public WEX::TestClass<MidiCiMessageTests>
{
public:

    BEGIN_TEST_CLASS(MidiCiMessageTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD(TestMuidEncodingIsSevenBitLeastSignificantFirst);
    TEST_METHOD(TestMuidRoundTrip);
    TEST_METHOD(TestParseDiscovery);
    TEST_METHOD(TestParseInvalidateMuid);
    TEST_METHOD(TestRejectsTruncatedMessage);
    TEST_METHOD(TestRejectsNonCapabilityInquiry);
    TEST_METHOD(TestPropertyExchangeHeaderLengthCannotExceedBuffer);
    TEST_METHOD(TestPropertyExchangeDataLengthCannotExceedBuffer);
    TEST_METHOD(TestParseValidPropertyExchangeRequest);
    TEST_METHOD(TestFuzzedMessagesNeverReportOffsetsPastTheBuffer);
    TEST_METHOD(TestBuildDiscoveryReplyBytes);
    TEST_METHOD(TestBuildDiscoveryReplyRefusesShortBuffer);
    TEST_METHOD(TestDiscoveryReplyParsesBack);
    TEST_METHOD(TestBuildPropertyExchangeReplyParsesBack);
    TEST_METHOD(TestPropertyExchangeRefusesHighBitPayload);
    TEST_METHOD(TestChunkArithmetic);
    TEST_METHOD(TestChunkedResourceReassembles);
    TEST_METHOD(TestChunkerEmitsEveryChunkExactlyOnce);
    TEST_METHOD(TestChunkerRejectsOutOfRangeChunkNumbers);

    TEST_METHOD(TestParseProfileInquiryReply);
    TEST_METHOD(TestProfileListCountCannotExceedBuffer);
    TEST_METHOD(TestParseSetProfileOnWithAndWithoutChannelCount);
    TEST_METHOD(TestParseProfileSpecificData);
    TEST_METHOD(TestProfileSpecificDataLengthCannotExceedBuffer);
    TEST_METHOD(TestBuildProfileMessagesParseBack);
    TEST_METHOD(TestBuildProfileInquiryReplyParsesBack);
    TEST_METHOD(TestParseNak);
    TEST_METHOD(TestBuildAcknowledgmentParsesBack);
    TEST_METHOD(TestBuildDiscoveryBytes);
    TEST_METHOD(TestBuildInvalidateMuidParsesBack);

private:

};
