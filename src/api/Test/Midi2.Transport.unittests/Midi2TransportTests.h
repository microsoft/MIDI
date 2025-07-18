// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

MODULE_SETUP(ModuleSetup);
bool ModuleSetup()
{
    winrt::init_apartment();
    return true;
}

class MidiTransportTests
    : public WEX::TestClass<MidiTransportTests>,
    public IMidiCallback
{
public:

    BEGIN_TEST_CLASS(MidiTransportTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSTransport.dll")
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
    TEST_METHOD(TestMidiKSTransport_UMP);
    TEST_METHOD(TestMidiKSTransport_ByteStream);
    TEST_METHOD(TestMidiKSTransport_Any);
    TEST_METHOD(TestMidiKSTransportCreationOrder_UMP);
    TEST_METHOD(TestMidiKSTransportCreationOrder_ByteStream);
    TEST_METHOD(TestMidiKSTransportCreationOrder_Any);
    TEST_METHOD(TestMidiKSTransportBidi_UMP);
    TEST_METHOD(TestMidiKSTransportBidi_ByteStream);
    TEST_METHOD(TestMidiKSTransportBidi_Any);
    TEST_METHOD(TestMidiKSIO_Latency_UMP);
    TEST_METHOD(TestMidiKSIO_Latency_ByteStream);
    TEST_METHOD(TestMidiKSIOSlowMessages_Latency_UMP);
    TEST_METHOD(TestMidiKSIOSlowMessages_Latency_ByteStream);

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
    TEST_METHOD(TestKsHandleWrapperQueryRemove_FilterHandle);
    TEST_METHOD(TestKsHandleWrapperSurpriseRemove_FilterHandle);
    TEST_METHOD(TestKsHandleWrapperQueryRemove_PinHandle);
    TEST_METHOD(TestKsHandleWrapperSurpriseRemove_PinHandle);

    STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG position, LONGLONG context)
    {
        if (m_MidiInCallback)
        {
            m_MidiInCallback(data, size, position, context);
        }
        return S_OK;
    }

    // The test library is not refcounted, stubbed.
    STDMETHODIMP QueryInterface(REFIID, void**) { return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() { return 1; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

private:
    void TestMidiTransport(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiTransportCreationOrder(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiTransportBidi(_In_ REFIID, _In_ MidiDataFormats);
    void TestMidiIO_Latency(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiSrvMultiClient(_In_ MidiDataFormats, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiSrvMultiClientBidi(_In_ MidiDataFormats, _In_ MidiDataFormats);

    std::function<void(PVOID, UINT32, LONGLONG, LONGLONG)> m_MidiInCallback;

    GUID m_SessionId{};
};
