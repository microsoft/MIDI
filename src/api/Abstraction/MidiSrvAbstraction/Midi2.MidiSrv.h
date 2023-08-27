// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidi2MidiSrv
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ MidiFlow, _In_ DWORD *, _In_opt_ IMidiCallback *));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:

    MidiClientHandle m_ClientHandle{ 0 };
    std::unique_ptr<CMidiXProc> m_MidiPump;
    DWORD m_MmcssTaskId{ 0 };
};


