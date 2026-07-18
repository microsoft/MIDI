---
layout: sdk_reference_page
title: MidiMessageConverter
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Helper class to convert MIDI 1.0 byte format messages into UMP messages
---

This class provides support for representing MIDI 1.0 messages in the Universal MIDI Packet format.

## Generic MIDI 1.0 Conversion Functions

| Function | Description |
| --------------- | ----------- |
| `ConvertMidi1Message (timestamp, group, statusByte)` | Convert MIDI 1.0 raw data into a `MidiMessage32` message |
| `ConvertMidi1Message (timestamp, group, statusByte, dataByte1)` | Convert MIDI 1.0 raw data into a `MidiMessage32` message |
| `ConvertMidi1Message (timestamp, group, statusByte, dataByte1, dataByte2)` | Convert MIDI 1.0 raw data into a `MidiMessage32` message |

## WinRT MIDI 1.0 System Message Conversion Functions

| Function | Description |
| --------------- | ----------- |
| `ConvertMidi1TimeCodeMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiTimeCodeMessage` message to `MidiMessage32`|
| `ConvertMidi1SongPositionPointerMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiSongPositionPointerMessage` message to `MidiMessage32`|
| `ConvertMidi1SongSelectMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiSongSelectMessage` message to `MidiMessage32`|
| `ConvertMidi1TuneRequestMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiTuneRequestMessage` message to `MidiMessage32`|
| `ConvertMidi1TimingClockMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiTimingClockMessage` message to `MidiMessage32`|
| `ConvertMidi1StartMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiStartMessage` message to `MidiMessage32`|
| `ConvertMidi1ContinueMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiContinueMessage` message to `MidiMessage32`|
| `ConvertMidi1StopMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiStopMessage` message to `MidiMessage32`|
| `ConvertMidi1ActiveSensingMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiActiveSensingMessage` message to `MidiMessage32`|
| `ConvertMidi1SystemResetMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiSystemResetMessage` message to `MidiMessage32`|

## WinRT MIDI 1.0 Channel Voice Message Conversion Functions

| Function | Description |
| --------------- | ----------- |
| `ConvertMidi1ChannelPressureMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiChannelPressureMessage` message to `MidiMessage32`|
| `ConvertMidi1NoteOffMessage (timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiNoteOffMessage` message to `MidiMessage32`|
| `ConvertMidi1NoteOnMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiNoteOnMessage` message to `MidiMessage32`|
| `ConvertMidi1PitchBendChangeMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiPitchBendChangeMessage` message to `MidiMessage32`|
| `ConvertMidi1PolyphonicKeyPressureMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiPolyphonicKeyPressureMessage` message to `MidiMessage32`|
| `ConvertMidi1ProgramChangeMessage(timestamp, group, originalMessage)` | Converts a WinRT MIDI 1.0 `MidiProgramChangeMessage` message to `MidiMessage32`|

## Byte Array Conversion Functions

| Static Method | Description |
| ------------- | ----------- |
| `ConvertMidi1MessageToUmpWords(group, originalMessage)` | Converts a WinRT `IMidiMessage` to a list of UMP words for the given group. |
| `ConvertMidi1CompleteMessageBytesToUmpWords(group, midi1Bytes, allowRunningStatus)` | Converts a sequence of raw MIDI 1.0 bytes to UMP words. Set `allowRunningStatus` to `true` to handle running status encoding. |
| `ConvertSingleGroupCompleteMessageUmpWordsToMidi1Bytes(umpWords)` | Converts a sequence of UMP words (all from one group) back to MIDI 1.0 bytes. |
| `ConvertHexByteStringToByteArray(hexByteString)` | Converts a space-separated or plain hex byte string (e.g., `"F0 41 10 F7"`) to a byte array. Useful for parsing SysEx strings from user input. |
