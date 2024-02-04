// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidi2MidiSrv
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ MidiFlow, _In_ PABSTRACTIONCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ GUID SessionId));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:

    MidiClientHandle m_ClientHandle{ 0 };
    std::unique_ptr<CMidiXProc> m_MidiPump;
    DWORD m_MmcssTaskId{ 0 };
};


