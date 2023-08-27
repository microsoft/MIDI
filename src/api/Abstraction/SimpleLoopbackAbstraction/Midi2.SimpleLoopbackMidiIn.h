// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiLoopbackDevice.h"

class CMidi2SimpleLoopbackMidiIn : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiIn,
        IMidiCallback>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ DWORD *, _In_opt_ IMidiCallback *));
    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);
    STDMETHOD(Cleanup)();

private:
    IMidiCallback* m_callback;

    // loopback is simple, so we only have the one device we're associated with
    MidiLoopbackDevice* m_midiDevice;

};


