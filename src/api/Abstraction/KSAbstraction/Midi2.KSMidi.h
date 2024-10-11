// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2KSMidi
{
public:

    HRESULT Initialize(_In_ LPCWSTR, _In_ MidiFlow, _In_ PABSTRACTIONCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG);
    HRESULT SendMidiMessage(_In_ PVOID , _In_ UINT , _In_ LONGLONG);
    HRESULT Shutdown();

private:
    std::unique_ptr<KSMidiInDevice> m_MidiInDevice;
    std::unique_ptr<KSMidiOutDevice> m_MidiOutDevice;
};

