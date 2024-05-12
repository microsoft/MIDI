// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

using namespace winrt::Microsoft::Devices::Midi2;


#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY (uint8_t)0x00
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION (uint8_t)0x01
#define MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION (uint8_t)0x02
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION (uint8_t)0x03
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION (uint8_t)0x04
#define MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_REQUEST (uint8_t)0x05
#define MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION (uint8_t)0x06


class MidiStreamMessageBuilderTests
    : public WEX::TestClass<MidiStreamMessageBuilderTests>
{
public:

    BEGIN_TEST_CLASS(MidiStreamMessageBuilderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Devices.Midi2.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.BluetoothMidiAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.DiagnosticsAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.MidiSrvAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.VirtualMidiAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.VirtualPatchBayAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"usbmidi2.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"MidiSrv.exe")
    END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestBuildEndpointNameNotificationLong);
    TEST_METHOD(TestBuildEndpointNameNotificationMedium);
    TEST_METHOD(TestBuildEndpointNameNotificationShort);
    TEST_METHOD(TestBuildProductInstanceIdNotificationShort);


private:


};

