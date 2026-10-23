// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// A device has one session with this PC at a time: in one direction, once per host here, and once
// per remote host. The transport declines a second one, and says why: a client entry records
// which session it is waiting for and connects once that ends, and a host keeps a short list of
// the remotes it turned away. Network MIDI Setup shows both.
//
// Runs the transport in the test process (see NetworkMidiTestInProcess.h) against a fake device
// on the loopback adapter, which uses one socket for everything, as many devices do.
class NetworkMidiSessionRefusalTests
    : public WEX::TestClass<NetworkMidiSessionRefusalTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiSessionRefusalTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    // The device is a client of a host here. A client entry for it is declined, records that host,
    // stops inviting, and connects once the device leaves that host.
    TEST_METHOD(ClientWaitsWhileTheRemoteIsAClientOfAHostHere);

    // This PC is connected to the device through one client entry. A second entry for it records
    // the first, stops inviting, and connects once the first disconnects.
    TEST_METHOD(ClientWaitsWhileAnotherClientEntryIsConnectedToTheRemote);

    // A host here turns away a second session from the same device, and a device this PC is a
    // client of. Each is listed once, newest first, with its reason.
    TEST_METHOD(HostListsTheRemotesItRefused);
};
