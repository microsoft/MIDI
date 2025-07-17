// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2KSMidiOut : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiOut>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD *, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG));
    STDMETHOD(Shutdown)();

private:

    std::unique_ptr<CMidi2KSMidi> m_midiDevice;
};


