// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2MidiSrvIn : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiIn>
{
public:
    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ GUID));
    STDMETHOD(Shutdown)();

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;
};


