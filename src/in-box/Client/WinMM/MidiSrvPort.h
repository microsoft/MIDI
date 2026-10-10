// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include <atomic>

#include "Feature_Servicing_MIDI2WinMMRemovalWithoutPortLock.h"

// base definition of the MIDIHDR structure,
// entries after dwFlags are for "driver" use, which
// we do not use. Some legacy apps pass in an incorrect structure,
// this definition ensures that adequate space is available for what
// wdmaud2.drv requires, while still being permissive of poorly behaving
// legacy applications.
typedef struct basemidihdr_tag {
    LPSTR       lpData;
    DWORD       dwBufferLength;
    DWORD       dwBytesRecorded;
    DWORD_PTR   dwUser;
    DWORD       dwFlags;
} BASEMIDIHDR;

class CMidiPort :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiCallback>
{
public:
    CMidiPort();
    ~CMidiPort();
    HRESULT RuntimeClassInitialize(_In_ GUID sessionId, _In_ std::wstring& interfaceId, _In_ MidiFlow flow, _In_ const MIDIOPENDESC* openDesc, _In_ DWORD_PTR flags);
    HRESULT Shutdown();
    HRESULT MidMessage(_In_ UINT msg, _In_  DWORD_PTR param1, _In_ DWORD_PTR param2);
    HRESULT ModMessage(_In_ UINT msg, _In_  DWORD_PTR param1, _In_ DWORD_PTR param2);
    bool IsFlow(_In_ MidiFlow flow);

    void NotifyInterfaceRemoval(std::wstring interfaceId)
    {
        if (Feature_Servicing_MIDI2WinMMRemovalWithoutPortLock::IsEnabled())
        {
            // A long SysEx send holds m_Lock for seconds, and device removal handling must not wait for it.
            if (m_RemovalInterfaceId == interfaceId)
            {
                m_RemovedWithoutPortLock = true;
            }

            return;
        }

        auto lock = m_Lock.lock();
        if (m_InterfaceId == interfaceId)
        {
            m_Invalidated = true;
        }
    }
    bool IsInvalidated()
    {
        if (Feature_Servicing_MIDI2WinMMRemovalWithoutPortLock::IsEnabled())
        {
            if (m_RemovedWithoutPortLock)
            {
                return true;
            }
        }

        auto lock = m_Lock.lock();
        return m_Invalidated;
    }

private:
    HRESULT Reset();
    HRESULT AddBuffer(_In_ LPMIDIHDR buffer, _In_ DWORD_PTR bufferSize);
    HRESULT Start();
    HRESULT Stop();
    HRESULT Close();

    // IMidiCallback, for receiving midi in messages from the service.
    STDMETHOD(Callback)(_In_ MessageOptionFlags, _In_ PVOID data, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context);

    HRESULT SendMidiMessage(_In_ UINT32 midiMessage);
    HRESULT SendLongMessage(_In_ LPMIDIHDR buffer);

    HRESULT CompleteLongBuffer(_In_ UINT message, _In_ LONGLONG position);
    
    void WinmmClientCallback(_In_ UINT msg, _In_ DWORD_PTR param1, _In_ DWORD_PTR param2);

    wil::critical_section m_Lock;

    bool m_Invalidated {false};

    // Set once before the port is published and never changed, so removal handling reads it without m_Lock.
    std::wstring m_RemovalInterfaceId;
    std::atomic<bool> m_RemovedWithoutPortLock {false};

    MIDIOPENDESC m_OpenDesc {0};
    DWORD_PTR m_Flags {0};

    std::wstring m_InterfaceId;

    MidiFlow m_Flow {MidiFlowIn};
    wil::unique_event m_Stopped{wil::EventOptions::None};
    wil::unique_event m_ExitCallback{wil::EventOptions::ManualReset};
    bool m_InCallback {false};
    bool m_Started {false};
    LONGLONG m_StartTime{0};
    LONGLONG m_qpcFrequency{0};

    wil::critical_section m_BuffersLock;
    bool m_IsInSysex{false};
    bool m_IsInRunningStatus {0};
    BYTE m_RunningStatus {0};
    std::queue<LPMIDIHDR> m_InBuffers;
    bool m_IsDiscardingSysex {true};
    wil::unique_event m_BuffersAdded{wil::EventOptions::None};

    std::unique_ptr<CMidi2MidiSrv> m_MidisrvTransport;
};
