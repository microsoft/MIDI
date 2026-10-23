// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Feature_Servicing_MIDI2LoopbackHardening. Every test here skips when the gate is off.
class LoopbackHardeningTests
    : public WEX::TestClass<LoopbackHardeningTests>
{
public:

    BEGIN_TEST_CLASS(LoopbackHardeningTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    // A second loopback under an association id already in use used to replace the first in the
    // transport's table. The first loopback's endpoints stayed behind, could not be removed, and
    // what was sent to them reached the second loopback.
    TEST_METHOD(TestCreateWithAssociationIdInUseIsRejected);

    // The service does not serialize configuration calls, so several apps creating at once all
    // passed the in-use check before any of them was added.
    TEST_METHOD(TestConcurrentCreatesWithOneAssociationIdCreateOneLoopback);

    // A rename rewrote endpoint names while a list in another call was copying them.
    TEST_METHOD(TestConcurrentRenamesAndListsKeepTheServiceRunning);

    // The side was found by searching the whole endpoint id for the A prefix, so a B unique id
    // containing it made both endpoints side A, and nothing got through either way.
    TEST_METHOD(TestSideBUniqueIdContainingSideAPrefixCarriesMidiBothWays);

    // Endpoint A is visible before B is created. An app which opened it in that moment was
    // connected to nothing in the B to A direction for as long as it kept it open.
    TEST_METHOD(TestEndpointOpenedWhileItsLoopbackIsCreatedReceivesMidi);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
};
