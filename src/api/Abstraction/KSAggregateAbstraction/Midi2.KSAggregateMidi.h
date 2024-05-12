// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


#define KSMIDI_PIN_MAP_ENTRY_COUNT 16   // if this were to change, it would foul up existing properties, so don't do that.

typedef struct {
    BOOL IsValid;
    UINT32 PinId;
} KSMIDI_PIN_MAP_ENTRY, * PKSMIDI_PIN_MAP_ENTRY;

// each index in the array is a group index
// valid groups do not need to be contiguous, but are due to how we process them
typedef struct {
    KSMIDI_PIN_MAP_ENTRY InputEntries[KSMIDI_PIN_MAP_ENTRY_COUNT]; // we use a fixed size array for these to make fetching simple and fast
    KSMIDI_PIN_MAP_ENTRY OutputEntries[KSMIDI_PIN_MAP_ENTRY_COUNT];
} KSMIDI_PIN_MAP, * PKSMIDI_PIN_MAP;




class CMidi2KSAggregateMidi
{
public:

    HRESULT Initialize(_In_ LPCWSTR, _In_ MidiFlow, _In_ PABSTRACTIONCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG);
    HRESULT SendMidiMessage(_In_ PVOID , _In_ UINT , _In_ LONGLONG);
    HRESULT Cleanup();

 //   HRESULT HandleCallback(_In_ uint8_t, _In_ PVOID, _In_ UINT, _In_ LONGLONG);

private:
    IMidiCallback* m_callback;

    // midi input proxies. These connect to the input device and include translation
    std::map<uint8_t, wil::com_ptr_nothrow<CMidi2KSAggregateMidiInProxy>> m_midiInDeviceGroupMap;

    // group map. key is the group index.
    std::map<uint8_t, std::unique_ptr<KSMidiOutDevice>> m_midiOutDeviceGroupMap;

    // translation support for aggregated UMP endpoints made up of MIDI 1.0 pins
    umpToBytestream m_UMP2BS;

};

