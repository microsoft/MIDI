// Copyright (c) Microsoft Corporation. All rights reserved.

#include "MidiSrvPort.h"

#pragma once

typedef ULONGLONG MidiPortHandle;

typedef struct _PORT_INFO
{
    UINT32 PortNumber {0};
    MidiFlow Flow{MidiFlowIn};
    std::wstring InterfaceId;
    std::wstring Name;

    MIDIINCAPSW MidiInCaps {0};
    MIDIOUTCAPSW MidiOutCaps {0};
} PORT_INFO;


#define MIDI_MAX_SESSION_NAME_LENGTH 50

// global singleton created on dll load, cleaned up on dllunload
class CMidiPorts :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IUnknown>
{
public:
    CMidiPorts();
    ~CMidiPorts();

    HRESULT RuntimeClassInitialize();
    HRESULT Shutdown();
    DWORD APIENTRY MidMessage(_In_ UINT deviceID, _In_ UINT msg, _In_ DWORD_PTR user, _In_ DWORD_PTR param1, _In_ DWORD_PTR param2);
    DWORD APIENTRY ModMessage(_In_ UINT deviceID, _In_ UINT msg, _In_ DWORD_PTR user, _In_ DWORD_PTR param1, _In_ DWORD_PTR param2);

private:
    HRESULT GetMidiDeviceCount(_In_ MidiFlow flow, _In_ UINT32& count);
    HRESULT GetDevCaps(_In_ MidiFlow flow, _In_ UINT portNumber, _In_ DWORD_PTR midiCaps);
    HRESULT Open(_In_ MidiFlow flow, _In_ UINT portNumber, _In_ const MIDIOPENDESC* midiOpenDesc, _In_ DWORD_PTR flags, _In_ MidiPortHandle* openedPort);
    HRESULT Close(_In_ MidiFlow flow, _In_ MidiPortHandle portHandle);
    HRESULT ForwardMidMessage(_In_ UINT msg, _In_ MidiPortHandle portHandle, _In_ DWORD_PTR param1, _In_ DWORD_PTR param2);
    HRESULT ForwardModMessage(_In_ UINT msg, _In_ MidiPortHandle portHandle, _In_ DWORD_PTR param1, _In_ DWORD_PTR param2);

    HRESULT GetOpenedPort(_In_ MidiFlow flow, _In_ MidiPortHandle portHandle, _In_ wil::com_ptr_nothrow<CMidiPort> &port);

    wil::critical_section m_Lock;

    std::unique_ptr<CMidi2MidiSrv> m_MidisrvTransport;

    // The session guid created for all ports opened by this client, a single guid is used for all sessions.
    GUID m_SessionId {0};
    // Default session name for winmm clients.
    std::wstring m_SessionName { };

    // map of midi port information ordered by the port number.
    // For each flow, the port number must be unique, but the port numbers
    // will repeat for different flows.
    // 2 entries in the array, midi in and midi out
    std::map<UINT32, PORT_INFO> m_MidiPortInfo[2];
    UINT m_MidiPortCount[2] {0};

    // map of the open midi clients ordered by the port handle, which is just the
    // address of the CMidiPort object for that client. 
    std::map<MidiPortHandle, wil::com_ptr_nothrow<CMidiPort>> m_OpenPorts;
};

