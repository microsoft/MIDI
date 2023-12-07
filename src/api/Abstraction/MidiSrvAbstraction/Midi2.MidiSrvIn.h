// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidi2MidiSrvIn : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiIn>
{
public:
    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PABSTRACTIONCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;
};


