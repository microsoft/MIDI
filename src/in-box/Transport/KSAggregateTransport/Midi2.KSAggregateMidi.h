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

    // Initialize and Shutdown for an open endpoint which also picks up pins the device adds or
    // removes while it is open. Implemented in Midi2.KSAggregateMidiPinMapUpdates.cpp.
    HRESULT InitializeFollowingPinMapChanges(
        _In_ LPCWSTR endpointDeviceInterfaceId,
        _In_ MidiFlow flow,
        _In_ PTRANSPORTCREATIONPARAMS creationParams,
        _Inout_ DWORD* mmCssTaskId,
        _In_opt_ IMidiCallback* callback,
        _In_ LONGLONG context);

    HRESULT ShutdownFollowingPinMapChanges();

 //   HRESULT HandleCallback(_In_ uint8_t, _In_ PVOID, _In_ UINT, _In_ LONGLONG);

private:
    wil::critical_section m_SendLock;

    IMidiCallback* m_callback;

    // midi input proxies. These connect to the input device and include translation
    std::map<uint8_t, wil::com_ptr_nothrow<CMidi2KSAggregateMidiInProxy>> m_midiInDeviceGroupMap;

    // midi output proxies. These also include outgoing (UMP to Bytestream) translation
    std::map<uint8_t, wil::com_ptr_nothrow<CMidi2KSAggregateMidiOutProxy>> m_midiOutDeviceGroupMap;


    std::map<std::wstring, std::unique_ptr<KsHandleWrapper>> m_openKsFilters;

    // Everything below is used only when following pin map changes

    HRESULT ReadPinMap(_In_ LPCWSTR endpointDeviceInterfaceId, _Out_ std::vector<KsaPinMap::Entry>& entries);
    HRESULT ApplyPinMap(_In_ std::vector<KsaPinMap::Entry> const& entries, _In_ bool stopAtFirstFailure, _Inout_ DWORD* mmCssTaskId);
    HRESULT OpenProxyForPin(_In_ KsaPinMap::Entry const& entry, _Inout_ DWORD* mmCssTaskId);
    void CloseProxyForPin(_In_ KsaPinMap::Entry const& entry);
    void CloseFiltersNoLongerUsed();

    std::wstring m_followedEndpointDeviceId{ };
    std::vector<KsaPinMap::Entry> m_openPins{ };     // one entry per open proxy
    LONGLONG m_proxyContext{ 0 };
    ULONG m_proxyBufferSize{ 0 };
    DWORD m_mmCssTaskIdForAddedPins{ 0 };

    // Declared last, so it ends, waiting for any change being applied, before the maps above are destroyed
    KsaPinMap::Subscription m_pinMapSubscription{ };


    // group map. key is the group index.
//    std::map<uint8_t, std::unique_ptr<KSMidiOutDevice>> m_midiOutDeviceGroupMap;

    // translation support for aggregated UMP endpoints made up of MIDI 1.0 pins
//    umpToBytestream m_UMP2BS;

//    wil::com_ptr_nothrow<IMidiDataTransform> m_Ump2BSTransform{ nullptr };
};

#endif
