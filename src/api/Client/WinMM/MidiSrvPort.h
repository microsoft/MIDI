// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

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

private:
    HRESULT Reset();
    HRESULT AddBuffer(_In_ LPMIDIHDR buffer, _In_ DWORD_PTR bufferSize);
    HRESULT Start();
    HRESULT Stop();
    HRESULT Close();

    // IMidiCallback, for receiving midi in messages from the service.
    STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context);

    HRESULT SendMidiMessage(_In_ UINT32 midiMessage);
    HRESULT SendLongMessage(_In_ LPMIDIHDR buffer);

    HRESULT CompleteLongBuffer(_In_ UINT message, _In_ LONGLONG position);
    
    void WinmmClientCallback(_In_ UINT msg, _In_ DWORD_PTR param1, _In_ DWORD_PTR param2);

    wil::critical_section m_Lock;

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
    BYTE m_RunningStatus {0};
    std::queue<LPMIDIHDR> m_InBuffers;
    wil::unique_event m_BuffersAdded{wil::EventOptions::None};

    std::unique_ptr<CMidi2MidiSrv> m_MidisrvTransport;
};
