// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiSessionTests
    : public WEX::TestClass<MidiSessionTests>
{
public:

    BEGIN_TEST_CLASS(MidiSessionTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Windows.Devices.Midi2.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.BluetoothMidiTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.DiagnosticsTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.MidiSrvTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.VirtualMidiTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.VirtualPatchBayTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"usbmidi2.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"MidiSrv.exe")
    END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

        //Generic Tests
    TEST_METHOD(TestCreateNewSession);
    TEST_METHOD(TestSessionList);
    TEST_METHOD(TestUpdateSessionName);


private:


};

