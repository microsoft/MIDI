// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
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
    STDMETHOD(AddClientSession(_In_ GUID SessionId, _In_ LPCWSTR SessionName));
    STDMETHOD(UpdateClientSessionName(_In_ GUID SessionId, _In_ LPCWSTR SessionName, _In_ DWORD ClientProcessId));
    STDMETHOD(RemoveClientSession(_In_ GUID SessionId));

    // These are called from within the service
    //STDMETHOD(AddClientEndpointConnection(_In_ GUID SessionId, _In_ LPCWSTR ConnectionEndpointInterfaceId));
    //STDMETHOD(RemoveClientEndpointConnection(_In_ GUID SessionId, _In_ LPCWSTR ConnectionEndpointInterfaceId));

    // This is called from the API
    STDMETHOD(GetSessionListJson(_Out_ BSTR* SessionList));
    STDMETHOD(GetSessionList)(_Out_ LPSAFEARRAY* SessionDetailsList);

    STDMETHOD(VerifyConnectivity)();

    STDMETHOD(Cleanup());

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;

    PVOID m_contextHandle{ nullptr };

    //GUID m_abstractionGuid;
};

