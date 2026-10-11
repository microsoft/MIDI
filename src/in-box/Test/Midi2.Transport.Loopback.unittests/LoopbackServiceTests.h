// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Service-side tests for what the service itself reports, using loopbacks as the endpoints to
// connect to and to create and remove.
class LoopbackServiceTests
    : public WEX::TestClass<LoopbackServiceTests>
{
public:

    BEGIN_TEST_CLASS(LoopbackServiceTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    // skips when Feature_Servicing_MIDI2SessionTrackerConnectionTime is off
    TEST_METHOD(TestSessionListReportsEachConnectionTime);

    // Runs the same with Feature_Servicing_MIDI2PortNumberCache on or off: the cache only saves
    // work, so the numbers have to come out unique either way.
    TEST_METHOD(TestPortNumbersStayUniqueAcrossCreateAndRemove);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
};
