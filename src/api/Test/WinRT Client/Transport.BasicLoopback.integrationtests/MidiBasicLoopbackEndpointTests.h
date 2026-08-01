// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiBasicLoopbackTests
    : public WEX::TestClass<MidiBasicLoopbackTests>
{
public:

    BEGIN_TEST_CLASS(MidiBasicLoopbackTests)
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

    // Verifies that a unique id containing invalid characters still results in a
    // successfully created loopback, because the manager cleans it up first.
    TEST_METHOD(TestCreateLoopbackWithGarbageUniqueId);


    // Issue GH1070: WinMM ports for a basic loopback must be able to be
    // re-opened after being closed.
    TEST_METHOD(TestReopenLegacyWinMMPorts);

private:


};
