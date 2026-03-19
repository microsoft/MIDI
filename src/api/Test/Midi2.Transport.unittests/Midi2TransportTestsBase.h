// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

#define TEST_APPID {0xc24cc593, 0xbc6b, 0x4726,{ 0xb5, 0x52, 0xbe, 0xc8, 0x2d, 0xed, 0xb6, 0x8c}}

BOOL GetEndpoints(MidiDataFormats dataFormat, BOOL midiOneInterface, REFIID requestedTransport, std::wstring& midiInInstanceId, std::wstring& midiOutInstanceId);
BOOL GetBidiEndpoint(MidiDataFormats dataFormat, REFIID requestedTransport, std::wstring& midiBidiInstanceId);
HRESULT GetEndpointGroupIndex(_In_ std::wstring midiDevice, _Inout_ DWORD& groupIndex);
HRESULT GetEndpointNativeDataFormat(_In_ std::wstring midiDevice, _Inout_ BYTE& nativeDataFormat);

void ErrorHandler(bool, wil::FailureInfo const& failure) WI_NOEXCEPT;

class MidiTransportTestsBase :
    public IMidiCallback
{
public:

    STDMETHOD(Callback)(_In_ MessageOptionFlags, _In_ PVOID data, _In_ UINT size, _In_ LONGLONG position, LONGLONG context)
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

protected:
    void TestMidiTransport(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiTransportCreationOrder(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL);
    void TestMidiTransportBidi(_In_ REFIID, _In_ MidiDataFormats);
    void TestMidiIO_Latency(_In_ REFIID, _In_ MidiDataFormats, _In_ BOOL, _In_ MessageOptionFlags);

    std::function<void(PVOID, UINT32, LONGLONG, LONGLONG)> m_MidiInCallback;

    GUID m_SessionId{};
};

