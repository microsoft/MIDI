// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class Midi2DriverTests
    : public WEX::TestClass<Midi2DriverTests>,
    public IMidiCallback
{
public:

    BEGIN_TEST_CLASS(Midi2DriverTests)
        TEST_CLASS_PROPERTY(L"ThreadingModel", L"MTA")
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestStandardMidiIO);
    TEST_METHOD(TestCyclicMidiIO);

    TEST_METHOD(TestStandardMidiIO_ManyMessages);
    TEST_METHOD(TestCyclicMidiIO_ManyMessages);

    TEST_METHOD(TestStandardMidiIO_Latency);
    TEST_METHOD(TestCyclicMidiIO_Latency);

    TEST_METHOD(TestStandardMidiIOSlowMessages_Latency);
    TEST_METHOD(TestCyclicMidiIOSlowMessages_Latency);

    Midi2DriverTests()
    {}

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
    void TestMidiIO(BOOL Cyclic);
    void TestMidiIO_ManyMessages(BOOL Cyclic);
    void TestMidiIO_Latency(BOOL Cyclic, BOOL DelayedMessages);
    
    std::function<void(PVOID, UINT32, LONGLONG)> m_MidiInCallback;
};

