// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidi2KSMidiBiDi : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiBiDi>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PABSTRACTIONCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG));
    STDMETHOD(SendMidiMessage(_In_ PVOID , _In_ UINT , _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:

    std::unique_ptr<CMidi2KSMidi> m_MidiDevice;
};

