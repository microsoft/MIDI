// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiComExtensionsTests
    : public WEX::TestClass<MidiComExtensionsTests>,
    public IMidiEndpointConnectionMessagesReceivedCallback
{
public:

    BEGIN_TEST_CLASS(MidiComExtensionsTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

        TEST_METHOD(TestSendReceiveMessages);

        STDMETHOD(MessagesReceived)(UINT64 timestamp, UINT32 wordCount, UINT32* messages)
        {
            if (m_midiInCallback)
            {
                m_midiInCallback(timestamp, wordCount, messages);
            }
            return S_OK;
        }

        STDMETHODIMP QueryInterface(REFIID, void**) { return S_OK; }
        STDMETHODIMP_(ULONG) AddRef() { return 1; }
        STDMETHODIMP_(ULONG) Release() { return 1; }

private:
    std::function<void(UINT64, UINT32, UINT32*)> m_midiInCallback;


};