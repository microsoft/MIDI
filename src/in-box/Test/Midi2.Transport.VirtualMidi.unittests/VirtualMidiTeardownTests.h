// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Service-side tests for removing a virtual device.
//
// Closing the device side of a virtual device makes the service remove both of its endpoints. If
// the service hangs while doing that, MIDI stops working for every app on the PC. Each test runs
// the close on its own thread and fails when it has not returned within a time limit, so a hang
// fails the test instead of stalling the whole test pass.
//
// Tests of gated behavior normally skip when the gate is off. These do not skip when
// Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock is disabled: a hang takes MIDI down for the
// whole PC, so it is a failure whichever code path the service takes.
//
// Running these against a service that hangs leaves MIDI hung on that PC until the MIDI service
// is restarted.
class VirtualMidiTeardownTests
    : public WEX::TestClass<VirtualMidiTeardownTests>
{
public:

    BEGIN_TEST_CLASS(VirtualMidiTeardownTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // the device side is the only connection, which is how an app that publishes a device and
    // then quits looks to the service
    TEST_METHOD(TestClosingDeviceSideDoesNotHangService);

    // an app is still connected to the client side when the device side goes away
    TEST_METHOD(TestClosingDeviceSideWithClientConnectedDoesNotHangService);
};
