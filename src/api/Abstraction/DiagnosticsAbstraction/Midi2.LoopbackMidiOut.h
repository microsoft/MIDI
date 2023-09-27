// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiLoopbackDevice.h"

class CMidi2LoopbackMidiOut : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiOut>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ DWORD *));
    STDMETHOD(SendMidiMessage(_In_ PVOID, _In_ UINT, LONGLONG));
    STDMETHOD(Cleanup)();

private:
    // loopback is simple, so we only have the one device we're associated with
    MidiLoopbackDevice* m_midiDevice;
};


