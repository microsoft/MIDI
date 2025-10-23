
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "WindowsMidiServices.h"

#ifndef MIDI_KSA_PIN_MAP_PROPERTY_H
#define MIDI_KSA_PIN_MAP_PROPERTY_H

typedef struct {
    UINT ByteCount;                         // total size of this struct, in bytes, including the Filter Id and its null terminator
    BYTE GroupIndex;                        // index (0-15) of the group this pin maps to
    UINT32 PinId;                           // KS Pin number
    MidiFlow PinDataFlow;                   // an input pin is MidiFlowIn, and from the user's perspective, a MIDI Output
    WCHAR FilterId[1];                      // full filter id for this pin
} KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY, * PKSAGGMIDI_PIN_MAP_PROPERTY_ENTRY;

typedef struct {
    UINT32 TotalByteCount;                          // total size of all entries and this header
    KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY Entries[1];   // List of property entries
} KSAGGMIDI_PIN_MAP_PROPERTY_VALUE, * PKSAGGMIDI_PIN_MAP_PROPERTY_VALUE;

#define SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER (sizeof(UINT32))
#define SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY_WITHOUT_STRING (sizeof(KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY) - sizeof(WCHAR))


// The binary is KSMIDI_PIN_MAP which contains 16 input and 16 output group maps
// this is used for when we create a UMP BIDI from a set of MIDI in and Out pins on a device
#define STRING_DEVPKEY_KsAggMidiGroupPinMap L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},16"
DEFINE_DEVPROPKEY(DEVPKEY_KsAggMidiGroupPinMap, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 16); // DEVPROP_TYPE_BINARY


// TODO: Code to get a filterid for a group from a pin map 





#endif
