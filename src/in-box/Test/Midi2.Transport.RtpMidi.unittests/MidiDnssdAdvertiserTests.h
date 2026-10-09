// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// DNS-SD registration through the Windows DNS client, shared by the Network MIDI 2.0 and RTP-MIDI
// transports. These register a made-up service type, _wmsprobe._udp, so no MIDI app lists them,
// and read each registration back with DnsServiceResolve. The DNS client announces them on the
// local network and sends a goodbye when each test withdraws its registration.
class MidiDnssdAdvertiserTests
    : public WEX::TestClass<MidiDnssdAdvertiserTests>
{
public:

    BEGIN_TEST_CLASS(MidiDnssdAdvertiserTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
    END_TEST_CLASS()

    TEST_METHOD(TestRegistersThePortAndText);
    TEST_METHOD(TestWithdrawingRemovesTheRegistration);
    TEST_METHOD(TestReportsTheLabelTheDnsClientChose);
    TEST_METHOD(TestWithdrawsOnlyItsOwnRegistration);
    TEST_METHOD(TestCancelingLeavesAnotherRegistrationAlone);
    TEST_METHOD(TestRefusesWhatCannotBeRegistered);
    TEST_METHOD(TestGivesUpOnceStopped);
};
