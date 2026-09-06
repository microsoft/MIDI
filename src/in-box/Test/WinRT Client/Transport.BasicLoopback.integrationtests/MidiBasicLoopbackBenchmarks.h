// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

// Benchmarks for the Basic Loopback transport.
//
// Each benchmark creates its own transient basic loopback, pushes a large number
// of MIDI 1.0 messages through it, and reports the total send/receive time and
// the average time per message. MidiClock is the time source throughout.
class MidiBasicLoopbackBenchmarks
    : public WEX::TestClass<MidiBasicLoopbackBenchmarks>,
    public IMidiEndpointConnectionMessagesReceivedCallback
{
public:

    BEGIN_TEST_CLASS(MidiBasicLoopbackBenchmarks)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    // 100,000 MIDI 1.0 UMP messages, sent 10 at a time through the COM extensions
    TEST_METHOD(BenchmarkComExtensionsSendReceive);

    // 100,000 MIDI 1.0 UMP messages using MidiMessage32 and the MessageReceived event
    TEST_METHOD(BenchmarkMidiMessage32SendReceive);

    // 100,000 MIDI 1.0 byte messages using WinMM midiOutShortMsg / midiInOpen
    TEST_METHOD(BenchmarkWinMMShortMessageSendReceive);

    // 100,000 MIDI 1.0 messages using the older Windows.Devices.Midi WinRT API,
    // which sends one message at a time and raises an event for each one received
    TEST_METHOD(BenchmarkWinRTMidi1SendReceive);


    // COM extensions callback. Forwards to whatever the running benchmark installed.
    STDMETHOD(MessagesReceived)(GUID sessionId, GUID connectionId, UINT64 timestamp, UINT32 wordCount, UINT32 const* messages)
    {
        if (m_midiInCallback)
        {
            m_midiInCallback(sessionId, connectionId, timestamp, wordCount, messages);
        }

        return S_OK;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject)
    {
        if (ppvObject == nullptr)
        {
            return E_POINTER;
        }

        *ppvObject = nullptr;

        if (riid == __uuidof(IMidiEndpointConnectionMessagesReceivedCallback) ||
            riid == __uuidof(IUnknown))
        {
            *ppvObject = static_cast<IMidiEndpointConnectionMessagesReceivedCallback*>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() { return 1; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

private:
    std::function<void(GUID, GUID, UINT64, UINT32, UINT32 const*)> m_midiInCallback;

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};

};
