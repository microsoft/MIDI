// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// A remote host which ends a session because its user did ("User terminated", 0x01) or because
// it is powering down (0x02) is left alone. The client entry stops, and connects again only when
// someone clicks Try again (the entry is connected again) or, for a host found through mDNS, when
// the host stops advertising and then advertises again. Any other reason reconnects as before.
//
// Runs the transport in the test process (see NetworkMidiTestInProcess.h) against a fake device.
class NetworkMidiRemoteByeTests
    : public WEX::TestClass<NetworkMidiRemoteByeTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiRemoteByeTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    // Bye 0x01 from a host connected by address: no new invitation until Try again
    TEST_METHOD(ClientStopsWhenTheHostsUserEndsTheSession);

    // the same for Bye 0x02
    TEST_METHOD(ClientStopsWhenTheHostPowersDown);

    // Any other Bye reason still reconnects at once, as it always has
    TEST_METHOD(ClientReconnectsAfterOtherByeReasons);

    // A host found through mDNS ends the session with 0x01. Its periodic announcements don't bring
    // the entry back. Taking the advertisement away and putting it back does.
    TEST_METHOD(ClientConnectsAgainWhenTheHostAdvertisesAgain);
};
