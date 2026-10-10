// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Service-side test for removing a loopback while apps still have both of its ends open.
//
// The service has to take the endpoints away from the open connections. If it hangs while doing
// that, MIDI stops working for every app on the PC. The work runs on its own thread and the test
// fails when it has not returned within a time limit, so a hang fails the test instead of
// stalling the whole test pass. Nothing here skips on a servicing gate: a hang is a failure
// whichever code path the service takes.
//
// Running this against a service that hangs leaves MIDI hung on that PC until the MIDI service
// is restarted.
class LoopbackTeardownTests
    : public WEX::TestClass<LoopbackTeardownTests>
{
public:

    BEGIN_TEST_CLASS(LoopbackTeardownTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestRemovingLoopbackWithBothSidesOpenDoesNotHangService);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
};
