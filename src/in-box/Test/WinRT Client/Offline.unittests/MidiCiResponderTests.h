// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiCiResponderTests
    : public WEX::TestClass<MidiCiResponderTests>
{
public:

    BEGIN_TEST_CLASS(MidiCiResponderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD(TestRepliesToBroadcastDiscovery);
    TEST_METHOD(TestReplyEchoesOutputPathId);
    TEST_METHOD(TestIgnoresMessagesAddressedElsewhere);
    TEST_METHOD(TestInvalidateMuidOnlyWhenItIsOurs);
    TEST_METHOD(TestShortReplyBufferIsReportedNotTruncated);
    TEST_METHOD(TestDoesNotReplyWithoutAMuid);
    TEST_METHOD(TestPropertyExchangeCapabilityBit);
    TEST_METHOD(TestGetPropertyDataIsHandedToTheCaller);
    TEST_METHOD(TestPropertyExchangeCapabilitiesReply);

private:

};
