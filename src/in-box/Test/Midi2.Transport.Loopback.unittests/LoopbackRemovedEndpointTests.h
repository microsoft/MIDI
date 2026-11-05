// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Feature_Servicing_MIDI2LoopbackRejectRemovedEndpoint. Every test here skips when the gate is off.
class LoopbackRemovedEndpointTests
    : public WEX::TestClass<LoopbackRemovedEndpointTests>
{
public:

    BEGIN_TEST_CLASS(LoopbackRemovedEndpointTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    // Opening an endpoint of a loopback that had been removed succeeded, and the app was connected
    // to nothing for as long as it kept it open.
    TEST_METHOD(TestOpeningAnEndpointOfARemovedLoopbackFailsWithNotFound);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
};
