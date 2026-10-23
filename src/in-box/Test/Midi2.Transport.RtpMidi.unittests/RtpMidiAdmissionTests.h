// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// What a host's session engine lets in. Anyone on the network can send an invitation, so these
// drive one engine with hand-made invitations from made-up addresses and watch what it answers.
class RtpMidiAdmissionTests
    : public WEX::TestClass<RtpMidiAdmissionTests>
{
public:

    BEGIN_TEST_CLASS(RtpMidiAdmissionTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // accepting is limited, so a device cycling invitations can't make endpoints come and go
    TEST_METHOD(TestAcceptsFromOneAddressAreLimited);
    TEST_METHOD(TestNewNamesFromOneAddressAreLimited);
    TEST_METHOD(TestNewNamesOverallAreLimited);
    TEST_METHOD(TestAcceptsOverallAreLimited);
    TEST_METHOD(TestHalfOpenInvitationsFromOneAddressAreLimited);
    TEST_METHOD(TestLoopbackIsNotLimited);

    // an invitation that is refused, held or never finished ends nothing that exists
    TEST_METHOD(TestRefusedInvitationEndsNothing);
    TEST_METHOD(TestUnfinishedInvitationEndsNothing);
    TEST_METHOD(TestInvitationNeverEndsOurOwnSession);
    TEST_METHOD(TestRepeatedInvitationIsAnsweredSparingly);
};
