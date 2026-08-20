// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiLegacyPortDeviceWatcherTests
    : public WEX::TestClass<MidiLegacyPortDeviceWatcherTests>
{
public:

    BEGIN_TEST_CLASS(MidiLegacyPortDeviceWatcherTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD(TestCreateAndEnumerate);
    TEST_METHOD(TestCreateAndEnumerateForAllFlows);

    TEST_METHOD(TestGetMethods);

    // Verifies Added is raised for every MIDI 1.0 port created while the watcher
    // is running. See the comment above the implementation for why this waits for
    // EnumerationCompleted first.
    TEST_METHOD(TestAddedRaisedForPortsCreatedWhileWatching);

private:


};


