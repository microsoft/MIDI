// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

typedef struct MIDI_CLIENT_PIPE
{
    std::wstring MidiDevice;
    MidiClientHandle ClientHandle{ 0 };

    MidiProtocol Protocol{ LegacyMidi };
    MidiFlow Flow{ MidiFlowIn };

    std::unique_ptr<CMidiXProc> MidiPump;
} MIDI_CLIENT_PIPE, * PMIDI_CLIENT_PIPE;

class CMidiClientPipe :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiCallback>
{
public:

    HRESULT AdjustForBufferingRequirements(_In_ PMIDISRV_CLIENTCREATION_PARAMS CreationParams);

    HRESULT Initialize(   _In_ handle_t BindingHandle,
                            _In_ HANDLE,
                            _In_ LPCWSTR,
                            _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                            _In_ PMIDISRV_CLIENT,
                            _In_ DWORD *,
                            _In_ wil::com_ptr_nothrow<CMidiDevicePipe>&);
    HRESULT Cleanup();

    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    std::wstring MidiDevice() { return m_ClientPipe.MidiDevice; }

private:
    MIDI_CLIENT_PIPE m_ClientPipe;

    wil::com_ptr_nothrow<CMidiDevicePipe> m_MidiDevicePipe;
};

