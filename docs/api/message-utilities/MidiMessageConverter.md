---
layout: api_page
title: MidiMessageConverter
parent: Message Utilities
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiMessageConverter

This class provides support for representing MIDI 1.0 messages in the Universal MIDI Packet format.

## Generic MIDI 1.0 Conversion Functions

| Function | Description |
| --------------- | ----------- |
| `ConvertMidi1Message(timestamp, group, statusByte)` | Convert MIDI 1.0 raw data into a `MidiMessage32` message |
| `ConvertMidi1Message(timestamp, group, statusByte, dataByte1)` | Convert MIDI 1.0 raw data into a `MidiMessage32` message |
| `ConvertMidi1Message(timestamp, group, statusByte, dataByte1, dataByte2)` | Convert MIDI 1.0 raw data into a `MidiMessage32` message |

## WinRT MIDI 1.0 System Message Conversion Functions

| Function | Description |
| --------------- | ----------- |
| `ConvertMidi1TimeCodeMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiTimeCodeMessage` message to `MidiMessage32`|
| `ConvertMidi1SongPositionPointerMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiSongPositionPointerMessage` message to `MidiMessage32`|
| `ConvertMidi1SongSelectMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiSongSelectMessage` message to `MidiMessage32`|
| `ConvertMidi1TuneRequestMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiTuneRequestMessage` message to `MidiMessage32`|
| `ConvertMidi1TimingClockMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiTimingClockMessage` message to `MidiMessage32`|
| `ConvertMidi1StartMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiStartMessage` message to `MidiMessage32`|
| `ConvertMidi1ContinueMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiContinueMessage` message to `MidiMessage32`|
| `ConvertMidi1StopMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiStopMessage` message to `MidiMessage32`|
| `ConvertMidi1ActiveSensingMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiActiveSensingMessage` message to `MidiMessage32`|
| `ConvertMidi1SystemResetMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiSystemResetMessage` message to `MidiMessage32`|

## WinRT MIDI 1.0 Channel Voice Message Conversion Functions

| Function | Description |
| --------------- | ----------- |
| `ConvertMidi1ChannelPressureMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiChannelPressureMessage` message to `MidiMessage32`|
| `ConvertMidi1NoteOffMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiNoteOffMessage` message to `MidiMessage32`|
| `ConvertMidi1NoteOnMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiNoteOnMessage` message to `MidiMessage32`|
| `ConvertMidi1PitchBendChangeMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiPitchBendChangeMessage` message to `MidiMessage32`|
| `ConvertMidi1PolyphonicKeyPressureMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiPolyphonicKeyPressureMessage` message to `MidiMessage32`|
| `ConvertMidi1ProgramChangeMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiProgramChangeMessage` message to `MidiMessage32`|

## IDL

[MidiMessageConverter IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiMessageConverter.idl)

