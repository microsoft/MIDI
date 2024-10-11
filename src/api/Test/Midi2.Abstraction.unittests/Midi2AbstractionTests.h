// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

MODULE_SETUP(ModuleSetup);
bool ModuleSetup()
{
    winrt::init_apartment();
    return true;
}

class MidiAbstractionTests
    : public WEX::TestClass<MidiAbstractionTests>,
    public IMidiCallback
{
public:

    BEGIN_TEST_CLASS(MidiAbstractionTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.MidiSrvAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"usbmidi2.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"midisrv.exe")
        END_TEST_CLASS()

        TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestMidiKSAbstraction_UMP);
    TEST_METHOD(TestMidiKSAbstraction_ByteStream);
    TEST_METHOD(TestMidiKSAbstraction_Any);
    TEST_METHOD(TestMidiKSAbstractionCreationOrder_UMP);
    TEST_METHOD(TestMidiKSAbstractionCreationOrder_ByteStream);
    TEST_METHOD(TestMidiKSAbstractionCreationOrder_Any);
    TEST_METHOD(TestMidiKSAbstractionBiDi_UMP);
    TEST_METHOD(TestMidiKSAbstractionBiDi_ByteStream);
    TEST_METHOD(TestMidiKSAbstractionBiDi_Any);
    TEST_METHOD(TestMidiKSIO_Latency_UMP);
    TEST_METHOD(TestMidiKSIO_Latency_ByteStream);
    TEST_METHOD(TestMidiKSIOSlowMessages_Latency_UMP);
    TEST_METHOD(TestMidiKSIOSlowMessages_Latency_ByteStream);

    TEST_METHOD(TestMidiSrvAbstraction_UMP);
    TEST_METHOD(TestMidiSrvAbstraction_ByteStream);
    TEST_METHOD(TestMidiSrvAbstraction_Any);
    TEST_METHOD(TestMidiSrvAbstraction_UMP_MidiOne);
    TEST_METHOD(TestMidiSrvAbstraction_ByteStream_MidiOne);
    TEST_METHOD(TestMidiSrvAbstraction_Any_MidiOne);
    TEST_METHOD(TestMidiSrvAbstractionCreationOrder_UMP);
    TEST_METHOD(TestMidiSrvAbstractionCreationOrder_ByteStream);
    TEST_METHOD(TestMidiSrvAbstractionCreationOrder_Any);
    TEST_METHOD(TestMidiSrvAbstractionCreationOrder_UMP_MidiOne);
    TEST_METHOD(TestMidiSrvAbstractionCreationOrder_ByteStream_MidiOne);
    TEST_METHOD(TestMidiSrvAbstractionCreationOrder_Any_MidiOne);
    TEST_METHOD(TestMidiSrvAbstractionBiDi_UMP);
    TEST_METHOD(TestMidiSrvAbstractionBiDi_ByteStream);
    TEST_METHOD(TestMidiSrvAbstractionBiDi_Any);
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
    TEST_METHOD(TestMidiSrvMultiClientBiDi_UMP_UMP);
    TEST_METHOD(TestMidiSrvMultiClientBiDi_ByteStream_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClientBiDi_Any_Any);
    TEST_METHOD(TestMidiSrvMultiClientBiDi_UMP_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClientBiDi_ByteStream_UMP);
    TEST_METHOD(TestMidiSrvMultiClientBiDi_Any_ByteStream);
    TEST_METHOD(TestMidiSrvMultiClientBiDi_ByteStream_Any);
    TEST_METHOD(TestMidiSrvMultiClientBiDi_Any_UMP);
    TEST_METHOD(TestMidiSrvMultiClientBiDi_UMP_Any);

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
    void TestMidiAbstraction(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiAbstractionCreationOrder(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiAbstractionBiDi(_In_ REFIID, _In_ MidiDataFormats);
    void TestMidiIO_Latency(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiSrvMultiClient(_In_ MidiDataFormats, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiSrvMultiClientBiDi(_In_ MidiDataFormats, _In_ MidiDataFormats);

    std::function<void(PVOID, UINT32, LONGLONG, LONGLONG)> m_MidiInCallback;

    GUID m_SessionId{};
};

