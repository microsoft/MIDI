// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSrvTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSrvTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.MidiSrvTransport",
        // {c22d26fd-947b-5df1-a0f8-bc62d26d188d}
        (0xc22d26fd,0x947b,0x5df1,0xa0,0xf8,0xbc,0x62,0xd2,0x6d,0x18,0x8d))
};

HRESULT GetMidiSrvBindingHandle(_In_ handle_t* BindingHandle);

class CMidi2MidiSrv
{
public:
    HRESULT Initialize();
    HRESULT Initialize(_In_ LPCWSTR, _In_ MidiFlow, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ GUID SessionId);
    HRESULT SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG);
    HRESULT Shutdown();

    // session tracker
    HRESULT AddClientSession(_In_ GUID sessionId, _In_ LPCWSTR sessionName);
    HRESULT UpdateClientSessionName(_In_ GUID sessionId, _In_ LPCWSTR sessionName);
    HRESULT RemoveClientSession(_In_ GUID sessionId);
    HRESULT GetSessionList(_Out_ LPWSTR* sessionList);
   // HRESULT GetSessionList(_Out_ LPSAFEARRAY* SessionDetailsList);
    BOOL VerifyConnectivity();

    // configuration manager
    HRESULT UpdateConfiguration(_In_ LPCWSTR configurationJson, _Out_ LPWSTR* responseJson);
    HRESULT GetTransportList(_Out_ LPWSTR* transportListJson);
    HRESULT GetTransformList(_Out_ LPWSTR* transformListJson);

private:

    PVOID m_ContextHandle{ nullptr };

    MidiClientHandle m_ClientHandle{ 0 };
    std::unique_ptr<CMidiXProc> m_MidiPump;
    DWORD m_MmcssTaskId{ 0 };
};


