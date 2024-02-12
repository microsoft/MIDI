// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidi2MidiSrvOut : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiOut>
{
public:
    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PABSTRACTIONCREATIONPARAMS, _In_ DWORD *, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;
};


