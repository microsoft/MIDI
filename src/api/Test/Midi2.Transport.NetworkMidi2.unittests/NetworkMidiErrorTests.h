// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Error handling, sequencing and recovery. Everything here drives the host into a state it
// would normally only reach on a lossy or misbehaving network.

class NetworkMidiErrorTests
    : public WEX::TestClass<NetworkMidiErrorTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiErrorTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    // Commands outside an established session, spec 6.13, 6.14, 7.1, 7.2.3 and 7.2.4
    TEST_METHOD(UmpDataOutsideSessionIsRefused);
    TEST_METHOD(RetransmitRequestOutsideSessionIsRefused);
    TEST_METHOD(RetransmitErrorOutsideSessionIsRefused);
    TEST_METHOD(SessionResetOutsideSessionIsRefused);
    TEST_METHOD(SessionResetReplyOutsideSessionIsRefused);
    TEST_METHOD(RefusalIsNotRepeatedForEveryCommandInOneDatagram);

    // Sequence handling, spec 7.1
    TEST_METHOD(SequenceGapTriggersRetransmitRequest);
    TEST_METHOD(RetransmitRequestStopsAfterNakCommandNotSupported);
    TEST_METHOD(RetransmitErrorEndsTheRequestsAndSessionSurvives);
    TEST_METHOD(SessionSurvivesUnrecoverableGap);
    TEST_METHOD(DuplicateSequenceNumbersAreIgnored);
    TEST_METHOD(EmptyUmpDataIsAcceptedAsKeepAlive);

    // Session reset, spec 6.13 and 6.14
    TEST_METHOD(SessionResetIsAcknowledged);

    // Unknown commands, spec 6.15
    TEST_METHOD(UnknownCommandCodeIsNaked);

    // Retransmit as a responder, spec 7.2
    TEST_METHOD(RetransmitRequestForUnknownSequenceIsAnswered);
};
