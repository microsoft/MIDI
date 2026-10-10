// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Service-side tests for a virtual device's endpoints while they are in use: apps connecting
// again, the in-use report to the device app, and names. The SDK tests cover the same behavior
// through the SDK. These run in the same build as the service.
//
// Each test skips when the gate for its behavior is off. The ones which close a virtual device
// also skip when Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock is off, because closing one
// hangs that service. VirtualMidiTeardownTests reports that hang.
class VirtualMidiEndpointTests
    : public WEX::TestClass<VirtualMidiEndpointTests>
{
public:

    BEGIN_TEST_CLASS(VirtualMidiEndpointTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    // an app can connect again after the last app connected to the device has gone
    TEST_METHOD(TestClientCanReconnectAfterTheLastClientCloses);

    // the device app is told whether any app is connected to its device
    TEST_METHOD(TestTransportDeclaresClientEndpointInUseCapability);
    TEST_METHOD(TestClientEndpointInUseFollowsAppConnections);

    // names
    TEST_METHOD(TestOverlongUnicodeNameIsCutOnCharacterBoundary);
    TEST_METHOD(TestEndpointNameFromDeviceIsDecodedAsUtf8);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
};
