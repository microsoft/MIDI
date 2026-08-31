// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2MidiSrvSessionTracker :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiSessionTracker>
{
public:
    STDMETHOD(Initialize());

    // These are called from the client API
    STDMETHOD(AddClientSession(_In_ GUID sessionId, _In_ LPCWSTR sessionName));
    STDMETHOD(UpdateClientSessionName(_In_ GUID sessionId, _In_ LPCWSTR sessionName));
    STDMETHOD(RemoveClientSession(_In_ GUID sessionId));

    // These are called from within the service
    //STDMETHOD(AddClientEndpointConnection(_In_ GUID SessionId, _In_ LPCWSTR ConnectionEndpointInterfaceId));
    //STDMETHOD(RemoveClientEndpointConnection(_In_ GUID SessionId, _In_ LPCWSTR ConnectionEndpointInterfaceId));

    // This is called from the SDK
    STDMETHOD(GetSessionList(_Out_ LPWSTR* sessionList));
    //STDMETHOD(GetSessionList)(_Out_ LPSAFEARRAY* SessionDetailsList);

    STDMETHOD(Shutdown());

    STDMETHOD_(BOOL,VerifyConnectivity)();

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;
};

