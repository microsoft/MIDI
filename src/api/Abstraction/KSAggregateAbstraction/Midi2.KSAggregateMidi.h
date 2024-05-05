// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


#define KSMIDI_PIN_MAP_ENTRY_COUNT 16

typedef struct {
    BOOL IsValid;
    UINT32 PinId;
} KSMIDI_PIN_MAP_ENTRY, * PKSMIDI_PIN_MAP_ENTRY;

typedef struct {
    KSMIDI_PIN_MAP_ENTRY InputEntries[KSMIDI_PIN_MAP_ENTRY_COUNT]; // we use a fixed size array for these
    KSMIDI_PIN_MAP_ENTRY OutputEntries[KSMIDI_PIN_MAP_ENTRY_COUNT];
} KSMIDI_PIN_MAP, * PKSMIDI_PIN_MAP;



class CMidi2KSAggregateMidi
{
public:

    HRESULT Initialize(_In_ LPCWSTR, _In_ MidiFlow, _In_ PABSTRACTIONCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG);
    HRESULT SendMidiMessage(_In_ PVOID , _In_ UINT , _In_ LONGLONG);
    HRESULT Cleanup();

private:
    // group map. key is the group index. This is only used when we create
    // an aggregated BiDi from a bunch of MIDI 1.0 pins
    std::map<uint8_t, std::unique_ptr<KSMidiInDevice>> m_midiInDeviceGroupMap;
    std::map<uint8_t, std::unique_ptr<KSMidiOutDevice>> m_midiOutDeviceGroupMap;
    bool m_useGroupMap{ false };


    // for aggregated UMP endpoints
    umpToBytestream m_UMP2BS;
    bytestreamToUMP m_BS2UMP;

};

