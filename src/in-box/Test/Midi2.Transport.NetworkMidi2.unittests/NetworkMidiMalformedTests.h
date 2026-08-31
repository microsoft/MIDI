// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Hostile and malformed input. These target the specific defect classes the parser hardening
// was written for, so they exist as much to stay green as to pass once.
//
// The pass condition for most of them is the same: the service must still be answering
// afterwards. A crashed or wedged service fails the survival check that ends each test.

class NetworkMidiMalformedTests
    : public WEX::TestClass<NetworkMidiMalformedTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiMalformedTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    // Packet framing
    TEST_METHOD(BadSignatureIsIgnored);
    TEST_METHOD(EmptyAndRuntDatagramsAreIgnored);
    TEST_METHOD(CommandHeaderSplitAcrossTheEndOfTheDatagram);

    // Declared lengths which do not match reality
    TEST_METHOD(PayloadLengthLongerThanDatagramIsRejected);
    TEST_METHOD(MaximumPayloadLengthWithNoPayload);
    TEST_METHOD(TrailingBytesAfterTheLastCommand);

    // Length arithmetic in string-bearing commands
    TEST_METHOD(InvitationNameLengthExceedsPayloadLength);
    TEST_METHOD(InvitationNameLengthIsMaximum);
    TEST_METHOD(InvitationWithOversizedStrings);
    TEST_METHOD(InvitationWithInvalidUtf8);
    TEST_METHOD(InvitationWithEmbeddedNulls);

    // The heap over-read class
    TEST_METHOD(UmpMessageClaimsMoreWordsThanSent);
    TEST_METHOD(UmpDataWithTruncatedFinalMessage);

    // Compound packet parsing
    TEST_METHOD(ValidCommandAfterUnknownCommandIsStillProcessed);
    TEST_METHOD(ByeFollowedByMoreCommandsDoesNotMisparse);

    // Resource pressure
    TEST_METHOD(OversizedDatagramIsHandled);
    TEST_METHOD(RapidInvitationsFromManyPortsAreBounded);

    // The configuration path, which is a separate attack surface from the packet parser above.
    // Anything can arrive on UpdateConfiguration, including payloads which are not JSON.
    TEST_METHOD(ConfigNonJsonPayloadsAreRejected);
    TEST_METHOD(ConfigMalformedJsonIsRejected);

    TEST_METHOD(ConfigStructurallyValidButWrongShapeIsRejected);

    TEST_METHOD(ConfigHostileStringValuesAreHandled);
    TEST_METHOD(ConfigDeeplyNestedJsonIsHandled);
    TEST_METHOD(ConfigHugePayloadIsHandled);
    TEST_METHOD(ServiceStillAnswersAfterConfigFuzzing);

    // Overall survival
    TEST_METHOD(ServiceSurvivesRandomFuzzing);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
};
