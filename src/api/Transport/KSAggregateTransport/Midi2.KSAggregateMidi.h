// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI2_KSAGGREGATEMIDI_H
#define MIDI2_KSAGGREGATEMIDI_H


class CMidi2KSAggregateMidi
{
public:

    HRESULT Initialize(_In_ LPCWSTR, _In_ MidiFlow, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG);
    HRESULT SendMidiMessage(_In_ MessageOptionFlags, _In_ PVOID , _In_ UINT , _In_ LONGLONG);
    HRESULT Shutdown();

 //   HRESULT HandleCallback(_In_ uint8_t, _In_ PVOID, _In_ UINT, _In_ LONGLONG);

private:
    wil::critical_section m_SendLock;

    IMidiCallback* m_callback;

    // midi input proxies. These connect to the input device and include translation
    std::map<uint8_t, wil::com_ptr_nothrow<CMidi2KSAggregateMidiInProxy>> m_midiInDeviceGroupMap;

    // midi output proxies. These also include outgoing (UMP to Bytestream) translation
    std::map<uint8_t, wil::com_ptr_nothrow<CMidi2KSAggregateMidiOutProxy>> m_midiOutDeviceGroupMap;


    std::map<std::wstring, std::unique_ptr<KsHandleWrapper>> m_openKsFilters;


    // group map. key is the group index.
//    std::map<uint8_t, std::unique_ptr<KSMidiOutDevice>> m_midiOutDeviceGroupMap;

    // translation support for aggregated UMP endpoints made up of MIDI 1.0 pins
//    umpToBytestream m_UMP2BS;

//    wil::com_ptr_nothrow<IMidiDataTransform> m_Ump2BSTransform{ nullptr };
};

#endif
