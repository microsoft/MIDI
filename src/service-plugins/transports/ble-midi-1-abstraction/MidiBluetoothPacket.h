// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

#define MIDI_BLE_SYSEX_START_BYTE   ((uint8_t)0xF0)
#define MIDI_BLE_SYSEX_STOP_BYTE    ((uint8_t)0xF7)


struct MidiBluetoothMultipleMessagePacketHeader
{
    uint8_t timetampHigh{};
};

struct MidiBluetoothMessage
{
    uint8_t timestampLow{};

    std::vector<uint8_t> midiData{};
};

