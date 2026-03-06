// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

class MidiSrvTransportTests
    : public WEX::TestClass<MidiSrvTransportTests>,
    public MidiTransportTestsBase
{
public:

    BEGIN_TEST_CLASS(MidiSrvTransportTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.MidiSrvTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"usbmidi2.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"midisrv.exe")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestMidiSrvTransport_UMP);
    TEST_METHOD(TestMidiSrvTransport_ByteStream);
    TEST_METHOD(TestMidiSrvTransport_Any);
    TEST_METHOD(TestMidiSrvTransport_UMP_MidiOne);
    TEST_METHOD(TestMidiSrvTransport_ByteStream_MidiOne);
    TEST_METHOD(TestMidiSrvTransport_Any_MidiOne);
    TEST_METHOD(TestMidiSrvTransportCreationOrder_UMP);
    TEST_METHOD(TestMidiSrvTransportCreationOrder_ByteStream);
    TEST_METHOD(TestMidiSrvTransportCreationOrder_Any);
    TEST_METHOD(TestMidiSrvTransportCreationOrder_UMP_MidiOne);
    TEST_METHOD(TestMidiSrvTransportCreationOrder_ByteStream_MidiOne);
    TEST_METHOD(TestMidiSrvTransportCreationOrder_Any_MidiOne);
    TEST_METHOD(TestMidiSrvTransportBidi_UMP);
    TEST_METHOD(TestMidiSrvTransportBidi_ByteStream);
    TEST_METHOD(TestMidiSrvTransportBidi_Any);
    TEST_METHOD(TestMidiSrvIO_Latency_UMP);
    TEST_METHOD(TestMidiSrvIO_Latency_ByteStream);
    TEST_METHOD(TestMidiSrvIOSlowMessages_Latency_UMP);
    TEST_METHOD(TestMidiSrvIOSlowMessages_Latency_ByteStream);
    TEST_METHOD(TestMidiSrvIO_Latency_UMP_WaitForSendComplete);
    TEST_METHOD(TestMidiSrvIO_Latency_ByteStream_WaitForSendComplete);
    TEST_METHOD(TestMidiSrvMultiClient_UMP_UMP);
    TEST_METHOD(TestMidiSrvMultiClient_ByteStream_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClient_Any_Any);
    TEST_METHOD(TestMidiSrvMultiClient_UMP_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClient_ByteStream_UMP);
    TEST_METHOD(TestMidiSrvMultiClient_Any_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClient_ByteStream_Any);
    TEST_METHOD(TestMidiSrvMultiClient_Any_UMP);
    TEST_METHOD(TestMidiSrvMultiClient_UMP_Any);
    TEST_METHOD(TestMidiSrvMultiClient_UMP_UMP_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClient_ByteStream_ByteStream_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClient_Any_Any_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClient_UMP_ByteStream_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClient_ByteStream_UMP_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClient_Any_ByteStream_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClient_ByteStream_Any_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClient_Any_UMP_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClient_UMP_Any_MidiOne);
    TEST_METHOD(TestMidiSrvMultiClientBidi_UMP_UMP);
    TEST_METHOD(TestMidiSrvMultiClientBidi_ByteStream_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClientBidi_Any_Any);
    TEST_METHOD(TestMidiSrvMultiClientBidi_UMP_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClientBidi_ByteStream_UMP);
    TEST_METHOD(TestMidiSrvMultiClientBidi_Any_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClientBidi_ByteStream_Any);
    TEST_METHOD(TestMidiSrvMultiClientBidi_Any_UMP);
    TEST_METHOD(TestMidiSrvMultiClientBidi_UMP_Any);

    TEST_METHOD(TestDriverDeviceInterfaceIdIsPresent);

    TEST_METHOD(MultiThreadedMidiSendTest);

    TEST_METHOD(TestKSAPortEnumeration);

private:
    void TestMidiSrvMultiClient(_In_ MidiDataFormats, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiSrvMultiClientBidi(_In_ MidiDataFormats, _In_ MidiDataFormats);

    void GetKSAMinMidiEndpoints    (std::vector<std::unique_ptr<MIDIU_DEVICE>> &midiInDevices,   std::vector<std::unique_ptr<MIDIU_DEVICE>> &midiOutDevices);
    HRESULT WaitForDeviceCount(std::vector<std::unique_ptr<MIDIU_DEVICE>> &midiInDevices, std::vector<std::unique_ptr<MIDIU_DEVICE>> &midiOutDevices, UINT expectedInCount, UINT expectedOutCount);
};


