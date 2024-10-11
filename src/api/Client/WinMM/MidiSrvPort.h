// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#define MIDI_NOTEOFF                0x80    // MIDI_NOTEOFF
#define MIDI_NOTEON                 0x90    // MIDI_NOTEON
#define MIDI_POLYAFTERTOUCH         0xa0    // MIDI_POLYAFTERTOUCH
#define MIDI_CONTROLCHANGE          0xb0    // MIDI_CONTROLCHANGE
#define MIDI_PROGRAMCHANGE          0xc0    // MIDI_PROGRAMCHANGE
#define MIDI_MONOAFTERTOUCH         0xd0    // MIDI_MONOAFTERTOUCH
#define MIDI_PITCHBEND              0xe0    // MIDI_PITCHBEND
#define MIDI_SYSEX                  0xf0    // MIDI_SYSEX
#define MIDI_TIMECODE               0xf1    // MIDI_TIMECODE
#define MIDI_SONGPOSITIONPOINTER    0xf2    // MIDI_SONGPOSITIONPOINTER
#define MIDI_SONGSELECT             0xf3    // MIDI_SONGSELECT
#define MIDI_TUNEREQUEST            0xf6    // MIDI_TUNEREQUEST
#define MIDI_EOX                    0xf7    // MIDI_EOX
#define MIDI_TIMINGCLOCK            0xf8    // MIDI_TIMINGCLOCK
#define MIDI_START                  0xfa    // MIDI_START
#define MIDI_CONTINUE               0xfb    // MIDI_CONTINUE
#define MIDI_STOP                   0xfc    // MIDI_STOP
#define MIDI_ACTIVESENSE            0xfe    // MIDI_ACTIVESENSE
#define MIDI_RESET                  0xff    // MIDI_RESET

#define MIDI_STATUSBYTEFILTER       0x80    // filter for MIDI status byte to remove channel info
                                            // a status byte must have the high bit set (0x80)
#define MIDI_SYSTEM_REALTIME_FILTER 0xf8    // filter for MIDI system realtime messages
                                            // a system realtime message must have all these bits set

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
    
    wil::com_ptr_nothrow<IMidiAbstraction> m_MidiAbstraction;
    wil::com_ptr_nothrow<IMidiIn> m_MidiIn;
    wil::com_ptr_nothrow<IMidiOut> m_MidiOut;
};