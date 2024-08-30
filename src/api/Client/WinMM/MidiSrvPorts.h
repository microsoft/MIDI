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
    HRESULT Cleanup();
    DWORD APIENTRY MidMessage(_In_ UINT uDeviceID, _In_ UINT uMsg, _In_ DWORD_PTR dwUser, _In_ DWORD_PTR dwParam1, _In_ DWORD_PTR dwParam2);
    DWORD APIENTRY ModMessage(_In_ UINT uDeviceID, _In_ UINT uMsg, _In_ DWORD_PTR dwUser, _In_ DWORD_PTR dwParam1, _In_ DWORD_PTR dwParam2);

private:
    HRESULT GetMidiDeviceCount(_In_ MidiFlow /*Flow*/, _In_ UINT32& Count);
    HRESULT GetDevCaps(_In_ MidiFlow Flow, _In_ UINT PortNumber, _In_ DWORD_PTR MidiCaps);
    HRESULT Open(_In_ MidiFlow Flow, _In_ UINT PortNumber, _In_ MIDIOPENDESC* MidiOpenDesc, _In_ DWORD_PTR Flags, _In_ MidiPortHandle* OpenedPort);
    HRESULT Close(_In_ MidiFlow Flow, _In_ MidiPortHandle PortHandle);
    HRESULT ForwardMidMessage(_In_ UINT uMsg, _In_ MidiPortHandle PortHandle, _In_ DWORD_PTR dwParam1, _In_ DWORD_PTR dwParam2);
    HRESULT ForwardModMessage(_In_ UINT uMsg, _In_ MidiPortHandle PortHandle, _In_ DWORD_PTR dwParam1, _In_ DWORD_PTR dwParam2);

    HRESULT GetOpenedPort(_In_ MidiFlow Flow, _In_ MidiPortHandle PortHandle, _In_ wil::com_ptr_nothrow<CMidiPort> &Port);

    wil::critical_section m_Lock;

    wil::com_ptr_nothrow<IMidiAbstraction> m_MidisrvAbstraction;
    wil::com_ptr_nothrow<IMidiSessionTracker> m_MidiSessionTracker;

    // The session guid created for all ports opened by this client, a single guid is used for all sessions.
    GUID m_SessionId {0};
    // Default session name for winmm clients.
    std::wstring m_SessionName {L"Winmm client session"};

    // map of midi port information ordered by the port number.
    // For each flow, the port number must be unique, but the port numbers
    // will repeat for different flows.
    // 2 entries in the array, midi in and midi out
    std::map<UINT32, PORT_INFO> m_MidiPortInfo[2];

    // map of the open midi clients ordered by the port handle, which is just the
    // address of the CMidiPort object for that client. 
    std::map<MidiPortHandle, wil::com_ptr_nothrow<CMidiPort>> m_OpenPorts;
};

