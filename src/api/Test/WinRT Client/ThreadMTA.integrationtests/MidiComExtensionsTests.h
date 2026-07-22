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
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

        TEST_METHOD(TestSendReceiveMessages);

        STDMETHOD(MessagesReceived)(GUID sessionId, GUID connectionId, UINT64 timestamp, UINT32 wordCount, UINT32* messages)
        {
            // don't have any cout statements in the flow for measurement purposes
            
            //    auto callbackThreadId = GetCurrentThreadId();
            //    std::cout << "Message received on lambda MessagesReceived COM callback thread: " << callbackThreadId << ", forwarding to m_midiInCallback." << std::endl;

            if (m_midiInCallback)
            {
                m_midiInCallback(sessionId, connectionId, timestamp, wordCount, messages);
            }
            return S_OK;
        }

        STDMETHODIMP QueryInterface(REFIID, void**) { return S_OK; }
        STDMETHODIMP_(ULONG) AddRef() { return 1; }
        STDMETHODIMP_(ULONG) Release() { return 1; }

private:
    std::function<void(GUID, GUID, UINT64, UINT32, UINT32*)> m_midiInCallback;

    void TestSendReceiveMessagesInternal();

    double m_totalTime{ 0 };
    uint64_t m_countWordsSent{ 0 };
};