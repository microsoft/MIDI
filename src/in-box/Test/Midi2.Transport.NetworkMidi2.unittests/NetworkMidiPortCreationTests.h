// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// MIDI 1.0 port creation for network endpoints, driven end to end against the real service.
//
// The remote in each case is a fake which answers Endpoint Discovery with real Function Block
// Info notifications, so these cover the path an actual device takes: discovery completes, the
// service reads the function blocks, and the ports follow from them. That is the case
// https://github.com/microsoft/MIDI/issues/1190 turned out to have several silent ways to fail.

namespace NetworkMidiTest
{
    class PortCreationTests
        : public WEX::TestClass<PortCreationTests>
    {
    public:

        BEGIN_TEST_CLASS(PortCreationTests)
            TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
            TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
        END_TEST_CLASS()

        TEST_CLASS_SETUP(ClassSetup);
        TEST_METHOD_SETUP(TestSetup);
        TEST_METHOD_CLEANUP(TestCleanup);

        // Windows is the client. A remote host which describes itself with one bidirectional
        // function block spanning several groups gets one MIDI 1.0 port per group in each
        // direction.
        TEST_METHOD(RemoteHostCompletingDiscoveryGetsAPortPerGroup);

        // Windows is the host. Same expectation for a remote client which connects in and
        // describes itself the same way.
        TEST_METHOD(RemoteClientCompletingDiscoveryGetsAPortPerGroup);

        // Asking for UMP only means no MIDI 1.0 ports, however well the remote describes itself.
        // This is the flag whose network default was wrong, so it is worth pinning in both
        // directions.
        TEST_METHOD(RemoteHostCreatesNoPortsWhenUmpOnly);

        // Changing the fallback count reaches an endpoint which is already up, with no
        // disconnect and no reconnect. https://github.com/microsoft/MIDI/issues/1190
        TEST_METHOD(ChangingTheFallbackPortCountAppliesWithoutReconnecting);

        // Renaming an endpoint renames its MIDI 1.0 ports too. The endpoint name and the port
        // name table are separate properties, and only the first was being written.
        TEST_METHOD(RenamingAnEndpointRenamesItsMidi1Ports);

        // A custom name survives a later port count change. The device interface keeps reporting
        // the name the remote supplied, so anything which rebuilds the ports from it loses the
        // custom one.
        TEST_METHOD(ChangingThePortCountKeepsACustomName);

        // Removing a customization puts the endpoint and its ports back to the remote's name.
        TEST_METHOD(RemovingACustomizationRevertsTheMidi1PortNames);

    private:

        MidiTest::DeviceNodeTracker m_deviceNodeTracker{ };
    };
}
