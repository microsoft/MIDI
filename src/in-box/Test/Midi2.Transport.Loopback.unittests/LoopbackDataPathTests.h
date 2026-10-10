// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Service-side tests for what a loopback does to the MIDI it carries: muting, and stopping a
// feedback loop. LoopbackConfigTests checks what the transport reports. These check what
// actually arrives. Each test skips when the gate for its behavior is off.
class LoopbackDataPathTests
    : public WEX::TestClass<LoopbackDataPathTests>
{
public:

    BEGIN_TEST_CLASS(LoopbackDataPathTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestMutedLoopbackDropsMessages);

    TEST_METHOD(TestFeedbackLoopMutesLoopback);
    TEST_METHOD(TestFeedbackLoopWithProtectionOffIsNotMuted);
    TEST_METHOD(TestSteadyTrafficIsNotMuted);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
};
