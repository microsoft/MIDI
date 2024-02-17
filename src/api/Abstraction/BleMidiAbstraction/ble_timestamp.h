// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef BLE_TIMESTAMP_H
#define BLE_TIMESTAMP_H



void ConvertMidiTimestampToBleTimestamp(_In_ uint64_t timestamp, _Out_ uint8_t bleTimestampHigh, _Out_ uint8_t bleTimestampLow);



#endif
