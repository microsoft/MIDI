
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "WindowsMidiServices.h"

#include <string>

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

// this exists so clients of this code don't have to deal with the FilterId string storage 
struct KSAPinMapEntryInternal
{
    uint8_t GroupIndex;
    uint32_t PinId;
    MidiFlow PinDataFlow;
    std::wstring FilterId;
};


#define SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER (sizeof(UINT32))
#define SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY_WITHOUT_STRING (sizeof(KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY) - sizeof(WCHAR))


// The binary is KSMIDI_PIN_MAP which contains 16 input and 16 output group maps
// this is used for when we create a UMP BIDI from a set of MIDI in and Out pins on a device
#define STRING_DEVPKEY_KsAggMidiGroupPinMap L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},16"
DEFINE_DEVPROPKEY(DEVPKEY_KsAggMidiGroupPinMap, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 16); // DEVPROP_TYPE_BINARY


// TODO: Code to read the pin map

inline HRESULT GetPinMapEntry(
    _In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& umpEndpointDeviceInfo,
    _In_ uint8_t groupIndex, 
    _In_ MidiFlow pinDataFlow, 
    _Inout_ KSAPinMapEntryInternal& entryToFill)
{
    auto refArray = internal::SafeGetSwdBinaryPropertyFromDeviceInformation(STRING_DEVPKEY_KsAggMidiGroupPinMap, umpEndpointDeviceInfo);

    if (refArray == nullptr)
    {
        return E_NOTFOUND;
    }

    // Read through the pin map until we find one with a matching group index and data flow

    auto data = refArray.Value();
    auto arraySize = data.size();
    auto propertyPointer = data.data();

    uint32_t bytesRead { 0 };

    auto pheader = (PKSAGGMIDI_PIN_MAP_PROPERTY_VALUE)(propertyPointer);
    bytesRead += sizeof(UINT32);    // the only value in the header that takes up room

    while (bytesRead < pheader->TotalByteCount && bytesRead < arraySize)
    {
        auto foundEntry = (PKSAGGMIDI_PIN_MAP_PROPERTY_ENTRY)(propertyPointer + bytesRead);

        if (foundEntry->GroupIndex == groupIndex && foundEntry->PinDataFlow == pinDataFlow)
        {
            size_t entrySizeWithoutString = sizeof(KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY) - 1;

            entryToFill.GroupIndex = foundEntry->GroupIndex;
            entryToFill.PinDataFlow = foundEntry->PinDataFlow;
            entryToFill.PinId = foundEntry->PinId;

            wchar_t* stringStart = foundEntry->FilterId;
            size_t stringLength = (foundEntry->ByteCount - entrySizeWithoutString) / sizeof(wchar_t);

            std::wstring filterId(stringStart, stringLength);
            entryToFill.FilterId = filterId;

            return S_OK;
        }

        bytesRead += foundEntry->ByteCount;
    }

    return E_NOTFOUND;
}


// TODO: Code to write the pin map





#endif
