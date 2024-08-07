#// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <chrono>

struct MidiSessionConnectionEntry
{
    std::wstring ConnectedEndpointInterfaceId;

    std::chrono::time_point<std::chrono::system_clock> EarliestStartTime;

    uint16_t InstanceCount{ 1 };

    // TODO: Time it was created

};

struct MidiSessionEntry
{
    GUID SessionId{ };
    DWORD ClientProcessId{ };
    //SYSTEMTIME StartTime;   // use GetSystemTime to generate
    std::chrono::time_point<std::chrono::system_clock> StartTime{ };

    std::wstring ProcessName;
    std::wstring SessionName;

    std::map<std::wstring, MidiSessionConnectionEntry> Connections;
    std::vector<MidiClientHandle> ClientHandles;
};


class CMidiSessionTracker : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiSessionTracker>
{
public:
    CMidiSessionTracker() {}
    ~CMidiSessionTracker() {}

    // needed for interface. A bit hacky
    STDMETHOD(Initialize)() { return S_OK; }


    STDMETHOD(Initialize)(_In_ std::shared_ptr<CMidiClientManager>& clientManager);
    
    // interface method. It's a dummy method because we need to pass up the processid and process name from the abstraction
    STDMETHOD(AddClientSession)(_In_ GUID SessionId, _In_ LPCWSTR SessionName);
    STDMETHOD(UpdateClientSessionName)(_In_ GUID SessionId, _In_ LPCWSTR SessionName, _In_ DWORD ClientProcessId);
    STDMETHOD(RemoveClientSession)(_In_ GUID SessionId);

    // These are called from within the service
    STDMETHOD(IsValidSession)(_In_ GUID SessionId, _In_ DWORD ClientProcessId);
    STDMETHOD(AddClientSessionInternal)(_In_ GUID SessionId, _In_ LPCWSTR SessionName, _In_ DWORD ClientProcessId, _In_ LPCWSTR ClientProcessName, _Out_opt_ PVOID* ContextHandle);
    STDMETHOD(AddClientEndpointConnection)(_In_ GUID SessionId, _In_ LPCWSTR ConnectionEndpointInterfaceId, _In_ MidiClientHandle ClientHandle);
    STDMETHOD(RemoveClientEndpointConnection)(_In_ GUID SessionId, _In_ LPCWSTR ConnectionEndpointInterfaceId, _In_ MidiClientHandle ClientHandle);

    STDMETHOD(RemoveClientSessionInternal)(_In_ PVOID ContextHandle);


    // This is called from the SDK
    STDMETHOD(GetSessionList)(_Out_ BSTR* SessionList);
    //STDMETHOD(GetSessionList)(_Out_ LPSAFEARRAY* SessionDetailsList);

    STDMETHOD(VerifyConnectivity)();

    STDMETHOD(Cleanup)();

private:
    std::weak_ptr<CMidiClientManager> m_clientManager;

    std::map<GUID, MidiSessionEntry, GUIDCompare> m_sessions{};
    std::map<PVOID, GUID> m_sessionContextHandles{};

};
