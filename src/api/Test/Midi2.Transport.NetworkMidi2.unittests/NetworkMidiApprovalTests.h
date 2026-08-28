// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Exercises the remote client approval flow end to end against the real service: a host
// configured for requireApproval, a raw UDP client which invites, the polling feed the settings
// app will use to notice the pending client, and each of the four user decisions.
//
// These create their own host rather than using the discovered one, because the policy has to be
// set at creation and the machine's configured host is whatever the user left it as.
class NetworkMidiApprovalTests : public WEX::TestClass<NetworkMidiApprovalTests>
{
public:
    BEGIN_TEST_CLASS(NetworkMidiApprovalTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    // The client is held pending and shows up in the polling feed rather than being accepted.
    TEST_METHOD(InvitationIsHeldPendingAndAppearsInEnumerateHosts);

    // The getPendingRemoteClients poll carries everything a user needs to decide, and drops an
    // entry once a decision has been made.
    TEST_METHOD(PendingRemoteClientsFeedCarriesEnoughToDecide);

    // "Allow just this time" - accepted now, nothing remembered.
    TEST_METHOD(ApproveOnceAcceptsTheWaitingClient);

    // "Allow always" - accepted now, and a later invitation from the same identity is accepted
    // without ever entering the pending state.
    TEST_METHOD(ApproveAlwaysIsRememberedForTheNextConnection);

    // "Deny until restart" - refused now, and still refused on a later attempt.
    TEST_METHOD(DenyUntilRestartRefusesTheWaitingClient);

    // "Deny always" - refused now, and a later invitation is refused immediately rather than
    // being held for another decision.
    TEST_METHOD(DenyAlwaysIsRememberedForTheNextConnection);

    // A decision naming an identity nobody is waiting under must not disturb the pending client.
    TEST_METHOD(DecisionForAnUnknownIdentityLeavesThePendingClientWaiting);

    // disconnectRemoteClient. Ends one remote's session without recording anything, which is
    // what separates it from a denial. There was previously no way to do this at all.
    TEST_METHOD(DisconnectRemoteClientEndsAnEstablishedSession);
    TEST_METHOD(DisconnectRemoteClientReleasesTheConnectionFromTheFeed);
    TEST_METHOD(DisconnectedRemoteClientIsNotRememberedAsDenied);
    TEST_METHOD(DisconnectRemoteClientForAnUnknownIdentityFailsCleanly);
    TEST_METHOD(DisconnectRemoteClientForAnUnknownHostFailsCleanly);
    TEST_METHOD(DisconnectRemoteClientWithoutAnIdentityFailsCleanly);

    // Every verb addresses a remote by its identity pair, so a remote which supplied only half
    // of one must not be listed as a connected device: the row could never be acted on.
    TEST_METHOD(RemoteWithAnIncompleteIdentityIsNotListedAsConnected);

    // Per-connection statistics on the host side, which the settings app polls. Latency is only
    // ever non-zero if the host actually pings an active session.
    TEST_METHOD(HostConnectionReportsStatisticsForAnActiveSession);

    // Two hosts cannot share a service instance name: it is the DNS-SD instance and the virtual
    // parent device id. The rejection has to carry a code and a description.
    TEST_METHOD(SecondHostWithTheSameServiceInstanceNameIsRejected);

    // The control for the test above. A distinct name must still be accepted.
    TEST_METHOD(HostWithAnUnusedServiceInstanceNameIsAccepted);

    // stopHost keeps the entry and its name; removeHost is what gives the name back.
    TEST_METHOD(RemovedHostReleasesItsServiceInstanceName);
    TEST_METHOD(RemovingAnUnknownHostReportsFailure);

    // https://github.com/microsoft/MIDI/issues/1149. Releasing the name inside the service is
    // not the same as taking the advertisement off the network. These ask mDNS directly, so a
    // pass means the record is really gone from the wire rather than just out of a local cache.
    TEST_METHOD(RemovedHostIsNoLongerAdvertisedOnTheNetwork);
    TEST_METHOD(StoppedHostIsNoLongerAdvertisedOnTheNetwork);

    // Two hosts cannot share a UDP port. That includes a manual port colliding with one the
    // system already handed to a host which asked for automatic allocation.
    TEST_METHOD(SecondHostWithTheSameManualPortIsRejected);
    TEST_METHOD(ManualPortMatchingAnAutomaticallyAssignedPortIsRejected);
    TEST_METHOD(HostWithAnUnusedManualPortIsAccepted);
    TEST_METHOD(HostWithAnOutOfRangePortIsRejected);
    TEST_METHOD(HostWithANonNumericPortIsRejected);

    // The identity a host puts on the wire has to be the identity it was configured with, or a
    // remote sees the advertised device and the connected device as two different things.
    TEST_METHOD(MaximumLengthIdentityStringsSurviveTheWire);

    // Removing a host straight after creating it races the endpoint creator thread, which
    // used to register the host after the entry had already been taken away. The host was
    // then unreachable and held its socket and service instance name until a restart. Several
    // tests in this file create and remove without waiting, so this leaked silently.
    TEST_METHOD(CreateThenImmediatelyRemoveLeavesNoHostBehind);
};
