// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

_Use_decl_annotations_
void ConvertMidiTimestampToBleTimestamp(uint64_t timestamp, uint8_t bleTimestampHigh, uint8_t bleTimestampLow)
{
    // BLE Timestamps are 13-bit values representing whole milliseconds.
    // bleTimestampHigh contains most significant 6 bits
    // bleTimestampLow contains least significant 7 bits


    // Convert timestamp to milliseconds. TODO: May want to cache the frequency

    uint64_t timestampMS = internal::ConvertTimestampToWholeMilliseconds(timestamp, internal::GetMidiTimestampFrequency());

    // take the lower 7 bits for timestamp low

    bleTimestampLow = (uint8_t)(timestampMS & 0x7F);

    // take the next 6 bits for timestamp high
    bleTimestampHigh = (uint8_t)((timestampMS >> 7) & 0x3F);

}


