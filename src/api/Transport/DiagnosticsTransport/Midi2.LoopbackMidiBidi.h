// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

//#include "MidiLoopbackBidiDevice.h"
//#include "MidiPingBidiDevice.h"

class CMidi2LoopbackMidiBidi : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiBidirectional,
        IMidiCallback>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ MessageOptionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Callback)(_In_ MessageOptionFlags, _In_ PVOID, _In_ UINT, _In_ LONGLONG, _In_ LONGLONG);
    STDMETHOD(Shutdown)();

private:
    IMidiCallback* m_Callback;
    LONGLONG m_Context;

    MidiLoopbackBidiDevice* m_LoopbackMidiDevice;
    MidiPingBidiDevice* m_PingMidiDevice;


    bool m_IsEndpointA; // true if A, false if B or ping
    bool m_IsPing{ false };
};


