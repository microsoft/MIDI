// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

MODULE_SETUP(ModuleSetup);
bool ModuleSetup()
{
    winrt::init_apartment();
    return true;
}

class Midi2ServiceTests
    : public WEX::TestClass<Midi2ServiceTests>,
    public IMidiCallback
{
public:

    BEGIN_TEST_CLASS(Midi2ServiceTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.MidiSrvTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"usbmidi2.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"MidiSrv.exe")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestMidiServiceClientRPC);
    TEST_METHOD(TestMidiServiceInvalidCreationParams);
    TEST_METHOD(TestMidiServiceFailedCreation);

    Midi2ServiceTests()
    {}

    STDMETHOD(Callback)(_In_ MessageOptionFlags, _In_ PVOID data, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
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
    std::function<void(PVOID, UINT32, LONGLONG, LONGLONG)> m_MidiInCallback;
};

