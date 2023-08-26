// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

class MidiAbstractionTests
    : public WEX::TestClass<MidiAbstractionTests>,
    public IMidiCallback
{
public:

    BEGIN_TEST_CLASS(MidiAbstractionTests)
        TEST_CLASS_PROPERTY(L"ThreadingModel", L"MTA")
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.MidiSrvAbstraction.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"midisrv.exe")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestMidiKSAbstraction);
    TEST_METHOD(TestMidiKSAbstractionCreationOrder);
    TEST_METHOD(TestMidiKSAbstractionBiDi);
    TEST_METHOD(TestMidiKSIO_Latency);

    TEST_METHOD(TestMidiSrvAbstraction);
    TEST_METHOD(TestMidiSrvAbstractionCreationOrder);
    TEST_METHOD(TestMidiSrvAbstractionBiDi);
    TEST_METHOD(TestMidiSrvIO_Latency);

    STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position)
    {
        if (m_MidiInCallback)
        {
            m_MidiInCallback(Data, Size, Position);
        }
        return S_OK;
    }

    // The test library is not refcounted, stubbed.
    STDMETHODIMP QueryInterface(REFIID, void**) { return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() { return 1; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

private:
    void TestMidiAbstraction(_In_ REFIID);
    void TestMidiAbstractionCreationOrder(_In_ REFIID);
    void TestMidiAbstractionBiDi(_In_ REFIID);
    void TestMidiIO_Latency(_In_ REFIID);

    std::function<void(PVOID, UINT32, LONGLONG)> m_MidiInCallback;
};

