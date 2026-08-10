// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

namespace NetworkMidiTest
{
    // "midisrv as a client" tests. Every test stands up a fake Network MIDI 2.0 host on
    // loopback, asks the service to connect a client to it, and then asserts on what the
    // service put on the wire. No external device and no mDNS, so these run in CI.
    class ClientTests
    {
        BEGIN_TEST_CLASS(ClientTests)
            TEST_CLASS_PROPERTY(L"TestClassification:Feature", L"NetworkMidi2ClientRole")
        END_TEST_CLASS()

        TEST_CLASS_SETUP(ClassSetup);
        TEST_CLASS_CLEANUP(ClassCleanup);

        // Connection establishment
        TEST_METHOD(ClientSendsInvitationToConfiguredHost);
        TEST_METHOD(ClientInvitationCarriesEndpointNameAndProductInstanceId);
        TEST_METHOD(ClientCompletesSessionWhenInvitationAccepted);
        TEST_METHOD(ClientStartsUmpSequenceNumbersAtZero);

        // Invitation retry and failure, spec 6.4
        TEST_METHOD(ClientRepeatsInvitationWhenHostSilent);
        TEST_METHOD(ClientCancelsInvitationWithCorrectByeReasonWhenHostNeverAnswers);

        // Invitation Reply: Pending, spec 6.6
        TEST_METHOD(ClientStopsInvitingAfterInvitationReplyPending);
        TEST_METHOD(ClientCompletesSessionAfterDelayedAcceptance);

        // Authentication refusal, spec 6.9 and 6.10
        TEST_METHOD(ClientWithdrawsWhenHostRequiresAuthentication);
        TEST_METHOD(ClientWithdrawsWhenHostRequiresUserAuthentication);

        // Host rejection
        TEST_METHOD(ClientAcceptsByeInsteadOfInvitationReply);

        // Liveness, spec 6.14
        TEST_METHOD(ClientAnswersHostPingWithMatchingId);
        TEST_METHOD(ClientSendsPingsWhenSessionIdle);

        // Session teardown, spec 6.16
        TEST_METHOD(ClientAnswersHostByeWithByeReply);
        TEST_METHOD(ClientSendsUserTerminatedByeOnExplicitDisconnect);
        TEST_METHOD(ClientRepeatsByeUntilByeReplyReceived);

        // Session reset, spec 6.13
        TEST_METHOD(ClientAnswersSessionResetWithReply);

        // Robustness. None of these may take the service down.
        TEST_METHOD(ClientSurvivesMalformedInvitationReply);
        TEST_METHOD(ClientSurvivesUnknownCommandCode);
        TEST_METHOD(ClientSurvivesTruncatedDatagram);
        TEST_METHOD(ClientIgnoresTrafficFromWrongSourcePort);
    };
}
