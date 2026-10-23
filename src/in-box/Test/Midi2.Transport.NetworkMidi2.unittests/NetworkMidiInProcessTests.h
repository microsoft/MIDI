// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Protocol behavior of the transport loaded into the test process (see NetworkMidiTestInProcess.h)
// and driven over the loopback adapter. Needs no service, no admin rights and no other device.
// Run with /p:TransportDll=<path> to try an older build.
class NetworkMidiInProcessTests
    : public WEX::TestClass<NetworkMidiInProcessTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiInProcessTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    // One remote address and port which is a client of a host here and the host for a client here
    // at the same time, as a device with a single socket is. The two connections must both stay,
    // and each must get its own datagrams. They used to share one entry, and the second replaced
    // the first.
    TEST_METHOD(HostAndClientForOneRemoteAddressAndPortAreSeparateConnections);

    // A remote with no session can put many commands in one datagram, from a forged address. Each
    // empty invitation used to get its own Bye.
    TEST_METHOD(DatagramOfEmptyInvitationsGetsAtMostOneReply);

    // The spec's limit is 1400 bytes. A larger datagram is dropped without a reply.
    TEST_METHOD(OversizedDatagramGetsNoReply);
};
