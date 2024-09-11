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

    PVOID ContextHandle{ nullptr };

    //SYSTEMTIME StartTime;   // use GetSystemTime to generate
    std::chrono::time_point<std::chrono::system_clock> StartTime{ };

    std::wstring ProcessName;
    std::wstring SessionName;

    std::map<std::wstring, MidiSessionConnectionEntry> Connections;
    std::vector<MidiClientHandle> ClientHandles;
};


class CMidiSessionTracker
{
public:
    CMidiSessionTracker() {}
    ~CMidiSessionTracker() {}

    STDMETHOD(Initialize)(_In_ std::shared_ptr<CMidiClientManager>& clientManager);
    

    // These are called from within the service
    STDMETHOD(IsValidSession)(_In_ GUID SessionId, _In_ DWORD ClientProcessId);
    STDMETHOD(AddClientSession)(_In_ GUID SessionId, _In_ LPCWSTR SessionName, _In_ DWORD ClientProcessId, _In_ wil::unique_handle& ClientProcessHandle, _Out_opt_ PVOID* ContextHandle);
    STDMETHOD(AddClientEndpointConnection)(_In_ GUID SessionId, _In_ DWORD ClientProcessId, _In_ LPCWSTR ConnectionEndpointInterfaceId, _In_ MidiClientHandle ClientHandle);
    STDMETHOD(UpdateClientSessionName)(_In_ GUID SessionId, _In_ LPCWSTR SessionName, DWORD ClientProcessId);
    STDMETHOD(RemoveClientSession)(_In_ GUID SessionId, _In_ DWORD ClientProcessId);
    STDMETHOD(RemoveClientEndpointConnection)(_In_ GUID SessionId, _In_ DWORD ClientProcessId, _In_ LPCWSTR ConnectionEndpointInterfaceId, _In_ MidiClientHandle ClientHandle);

    STDMETHOD(RemoveClientSessionInternal)(_In_ PVOID ContextHandle);

    // This is called from the SDK
    STDMETHOD(GetSessionList)(_Out_ BSTR* SessionList);
    //STDMETHOD(GetSessionList)(_Out_ LPSAFEARRAY* SessionDetailsList);

    STDMETHOD(VerifyConnectivity)();

    STDMETHOD(Cleanup)();

private:

    std::vector<MidiSessionEntry>::iterator FindSession(_In_ GUID sessionId, _In_ DWORD clientProcessId);
    std::vector<MidiSessionEntry>::iterator FindSessionForContextHandle(_In_ PVOID contextHandle);


    std::weak_ptr<CMidiClientManager> m_clientManager;

//    std::map<GUID, MidiSessionEntry, GUIDCompare> m_sessions{};

    std::vector<MidiSessionEntry> m_sessions{};
    wil::critical_section m_sessionsLock;

 //   std::map<PVOID, GUID> m_sessionContextHandles{};

};
