// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2KSAggregateMidiBidi :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiBidirectional>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ MessageOptionFlags, _In_ PVOID , _In_ UINT , _In_ LONGLONG));
    STDMETHOD(Shutdown)();

private:
    std::wstring m_endpointDeviceId{};
    std::atomic<uint64_t> m_countMidiMessageSent{};

    std::unique_ptr<CMidi2KSAggregateMidi> m_midiDevice;



};

