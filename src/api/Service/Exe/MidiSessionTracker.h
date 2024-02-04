#// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

struct MidiSessionConnectionEntry
{
    std::wstring ConnectedEndpointInterfaceId;

    SYSTEMTIME EarliestStartTime;

    uint16_t InstanceCount{ 1 };

    // TODO: Time it was created

};

struct MidiSessionEntry
{
    GUID SessionId;
    DWORD ClientProcessId;
    SYSTEMTIME StartTime;   // use GetSystemTime to generate

    std::wstring ProcessName;
    std::wstring SessionName;

    std::map<std::wstring, MidiSessionConnectionEntry> Connections;
};


class CMidiSessionTracker : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiSessionTracker>
{
public:

    CMidiSessionTracker() {}
    ~CMidiSessionTracker() {}

    HRESULT Initialize();

    // These are called from the API
    HRESULT AddClientSession(_In_ GUID SessionId, _In_ DWORD ClientProcessId, _In_ LPCWSTR ProcessName, _In_ LPCWSTR SessionName);
    HRESULT RemoveClientSession(_In_ GUID SessionId);

    // These are called from within the service
    HRESULT AddClientEndpointConnection(_In_ GUID SessionId, _In_ LPCWSTR ConnectionEndpointInterfaceId);
    HRESULT RemoveClientEndpointConnection(_In_ GUID SessionId, _In_ LPCWSTR ConnectionEndpointInterfaceId);

    // This is called from the API
    HRESULT GetSessionListJson(_Out_ BSTR* SessionList);

    HRESULT Cleanup();

private:
    std::map<GUID, MidiSessionEntry, GUIDCompare> m_sessions{};


};
