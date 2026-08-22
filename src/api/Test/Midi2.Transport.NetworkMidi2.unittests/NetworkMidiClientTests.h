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

        // A Ping Reply is the only round trip the transport can time. Pinging only when the
        // link had gone quiet meant a session actually carrying MIDI never measured latency.
        TEST_METHOD(ClientPingsEvenWhileTheHostIsSendingTraffic);
        TEST_METHOD(ClientReportsLatencyOnceThePingIsAnswered);
        TEST_METHOD(ReportedLatencySurvivesBeingReadTwice);

        // Session teardown, spec 6.16
        TEST_METHOD(ClientAnswersHostByeWithByeReply);
        TEST_METHOD(ClientSendsUserTerminatedByeOnExplicitDisconnect);
        TEST_METHOD(ClientRepeatsByeUntilByeReplyReceived);

        // Reconnect after the remote host goes away on its own, by rebooting or by dropping off
        // the network. The definition has to go back to the endpoint creator, and a disconnect
        // the user asked for must not.
        TEST_METHOD(ClientReconnectsAfterHostSendsBye);
        TEST_METHOD(ClientReconnectsAfterHostStopsResponding);
        TEST_METHOD(ClientDoesNotReconnectAfterUserDisconnect);
        TEST_METHOD(ClientConnectsToHostWhichComesOnlineLater);

        // Session reset, spec 6.13
        TEST_METHOD(ClientAnswersSessionResetWithReply);

        // Robustness. None of these may take the service down.
        TEST_METHOD(ClientSurvivesMalformedInvitationReply);
        TEST_METHOD(ClientSurvivesUnknownCommandCode);
        TEST_METHOD(ClientSurvivesTruncatedDatagram);
        TEST_METHOD(ClientIgnoresTrafficFromWrongSourcePort);

        // enumerateClients is driven by the configured definitions, so a disconnect which only
        // removed the live client left the entry being reported forever.
        TEST_METHOD(DisconnectRemovesTheEntryFromEnumerateClients);
        TEST_METHOD(DisconnectingAnEntryWhichNeverConnectedSucceeds);
        TEST_METHOD(DisconnectingAnUnknownEntryFailsCleanly);

        // Removing an entry while its client is still being built used to register the client
        // anyway. Nothing then listed it, because enumerateClients is definition-driven, and
        // nothing ever disconnected it, so it kept a socket and a live MIDI endpoint for the
        // lifetime of the service. That is what left orphaned endpoints behind after a run.
        TEST_METHOD(DisconnectDuringClientCreationLeavesNoLiveClient);

        // A name the user supplied when creating the connection has to be on the endpoint from
        // the moment it exists. Creating it under the remote's name and renaming afterwards
        // would churn the endpoint and its MIDI 1.0 ports.
        TEST_METHOD(CustomEndpointNameIsAppliedWhenTheEndpointIsCreated);

        // connectMdns. The service has always been able to follow an mDNS-discovered host, but
        // there was no command to create such an entry: connectDirect was the only one.
        TEST_METHOD(ConnectMdnsCreatesAnEntryMatchedByDeviceId);
        TEST_METHOD(ConnectMdnsWithoutAMatchIdFailsCleanly);
        TEST_METHOD(ConnectMdnsForAnExistingEntryRearmsIt);
    };
}
