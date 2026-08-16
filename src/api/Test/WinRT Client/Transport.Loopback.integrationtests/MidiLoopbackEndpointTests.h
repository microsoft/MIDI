// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiLoopbackEndpointTests
    : public WEX::TestClass<MidiLoopbackEndpointTests>
{
public:

    BEGIN_TEST_CLASS(MidiLoopbackEndpointTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    //TEST_CLASS_SETUP(ClassSetup);
    //TEST_CLASS_CLEANUP(ClassCleanup);

    //TEST_METHOD_SETUP(TestSetup);
    //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestCreateLoopback);
    TEST_METHOD(TestCreateLegacyPorts);
    TEST_METHOD(TestUmpSendReceive);

    // Verifies that unique ids containing invalid characters on both the A and B
    // sides still result in a successfully created loopback, because the manager
    // cleans them up first.
    TEST_METHOD(TestCreateLoopbackWithGarbageUniqueIds);

    // Muting, unmuting, and enumerating active loopbacks
    TEST_METHOD(TestMuteLoopback);
    TEST_METHOD(TestUnmuteAfterMute);
    TEST_METHOD(TestListActiveLoopbacks);

    // A loopback has four WinMM ports (source and destination for both A and B),
    // all of which must be re-openable after being closed.
    TEST_METHOD(TestReopenLegacyWinMMPorts);

    TEST_METHOD(TestUnicodeGtbAndDeviceNames);
    TEST_METHOD(TestOverlongUnicodeDeviceNameIsTruncatedOnCharacterBoundary);


private:


};