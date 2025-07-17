// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>


class MidiUMPToBSTransformTests
    : public WEX::TestClass<MidiUMPToBSTransformTests>,
    public IMidiCallback
{
public:

    BEGIN_TEST_CLASS(MidiUMPToBSTransformTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.BS2UMPTransform.dll")
    END_TEST_CLASS()

    //TEST_CLASS_SETUP(ClassSetup);
    //TEST_CLASS_CLEANUP(ClassCleanup);

    //TEST_METHOD_SETUP(TestSetup);
    //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestChannelVoiceMessages);

    void InternalTestMessages(
        _In_ std::vector<uint32_t> words,
        _In_ std::vector<uint8_t> const expectedBytes);

    STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position, LONGLONG Context)
    {
        if (m_MidiInCallback)
        {
            m_MidiInCallback(Data, Size, Position, Context);
        }
        return S_OK;
    }

    // The test library is not refcounted, stubbed.
    STDMETHODIMP QueryInterface(REFIID, void**) { return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() { return 1; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

private:

    std::function<void(PVOID, UINT, LONGLONG, LONGLONG)> m_MidiInCallback;
};