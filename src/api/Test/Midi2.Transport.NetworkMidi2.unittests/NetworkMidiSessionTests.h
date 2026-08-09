// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Protocol-level conformance tests. These speak raw UDP to a Network MIDI 2.0 host provided by
// the running MIDI service, acting as a Client. No COM or WinRT MIDI interfaces are involved.
//
// Requires a started host. Set MIDI_NET2UDP_TEST_HOST and MIDI_NET2UDP_TEST_PORT to point at a
// specific one, otherwise the local host is discovered over mDNS. Tests skip, rather than fail,
// when no host is available.

class NetworkMidiSessionTests
    : public WEX::TestClass<NetworkMidiSessionTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiSessionTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    // Discovery and reachability
    TEST_METHOD(HostIsDiscoverable);
    TEST_METHOD(HostAdvertisesRequiredTxtRecords);

    // Session establishment, spec 6.4 and 6.5
    TEST_METHOD(InvitationIsAccepted);
    TEST_METHOD(InvitationReplyCarriesNameAndProductInstanceId);
    TEST_METHOD(RepeatInvitationOnEstablishedSessionIsAccepted);
    TEST_METHOD(InvitationWithoutProductInstanceIdIsRefused);

    // Liveness, spec 6.11
    TEST_METHOD(PingIsAnswered);
    TEST_METHOD(PingReplyEchoesTheSamePingId);
    TEST_METHOD(HostSendsPingsToKeepSessionAlive);

    // Teardown, spec 6.16
    TEST_METHOD(ByeIsAnsweredWithByeReply);
    TEST_METHOD(SessionCanBeReestablishedAfterBye);
};
