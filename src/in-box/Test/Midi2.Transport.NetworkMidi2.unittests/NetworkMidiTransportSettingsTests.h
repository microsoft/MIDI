// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The settings which apply to the transport as a whole. Every one of them is corrected rather
// than refused, so these are mostly about proving that a bad value cannot stop the transport
// from running with something sensible.

class NetworkMidiTransportSettingsTests
    : public WEX::TestClass<NetworkMidiTransportSettingsTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiTransportSettingsTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(SettingsAreReportedBack);
    TEST_METHOD(ValuesAboveTheRangeAreClampedToTheMaximum);
    TEST_METHOD(ValuesBelowTheRangeAreClampedToTheMinimum);
    TEST_METHOD(WrongTypesFallBackToTheDefault);
    TEST_METHOD(MalformedNumbersFallBackToTheDefault);
    TEST_METHOD(AbsentSettingsAreLeftAlone);
    TEST_METHOD(ProductInstanceIdIsNoLongerATransportSetting);

    // Wrongly typed values anywhere in a host or client entry. Every one of these used to throw
    // out of a COM boundary rather than being refused.
    TEST_METHOD(WrongTypesInAHostEntryDoNotThrow);
    TEST_METHOD(WrongTypesInAClientEntryDoNotThrow);
    TEST_METHOD(WrongTypesInAnAllowedClientsArrayDoNotThrow);
    TEST_METHOD(OverlongServiceInstanceNameIsRejected);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
};
