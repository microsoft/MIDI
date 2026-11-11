// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiBasicLoopbackStressTests
    : public WEX::TestClass<MidiBasicLoopbackStressTests>
{
public:

    BEGIN_TEST_CLASS(MidiBasicLoopbackStressTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    // The same loopback, by unique identifier, many times in a row
    TEST_METHOD(TestCreateAndRemoveSameLoopbackRepeatedly);

    // Several clients connected and one of them flooding the loopback when it is removed
    TEST_METHOD(TestRemoveWhileReceiversConnectedAndSending);

    // Connections left open on a removed loopback must not disturb the one created in its place
    TEST_METHOD(TestRecreateWhileOldConnectionsStillOpen);

    // The same, for WinMM handles on the loopback's MIDI 1.0 ports
    TEST_METHOD(TestRemoveWhileWinMMPortsOpenAndSending);

    // Issue #1081: the association id of a MIDI 2.0 loopback, and of a basic loopback already removed
    TEST_METHOD(TestRemoveWithAssociationIdItDoesNotOwnIsRejected);

    // The service matches endpoints to remove by the start of their instance id
    TEST_METHOD(TestRemovingLoopbackLeavesLoopbackWithLongerUniqueIdAlone);

    // "TRANSPORT" would give a loopback the same device id as the transport's own parent device
    TEST_METHOD(TestUniqueIdOfTransportParentIsRejected);

    // A listener comes and goes while another application sends without a break
    TEST_METHOD(TestListenerReopenedWhileAnotherClientSends);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};

};
