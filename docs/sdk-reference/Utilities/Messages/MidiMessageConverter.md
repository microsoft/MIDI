---
layout: sdk_reference_page
title: MidiMessageConverter
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Helper class to convert MIDI 1.0 byte format messages into UMP messages
---

This class turns MIDI 1.0 messages into Universal MIDI Packets, and back.

## MIDI 1.0 bytes to UMP

| Static Method | Description |
| --------------- | ----------- |
| `ConvertMidi1Message(timestamp, group, statusByte)` | Turns a MIDI 1.0 message that has only a status byte into a `MidiMessage32` |
| `ConvertMidi1Message(timestamp, group, statusByte, dataByte1)` | Turns a MIDI 1.0 message with one data byte into a `MidiMessage32` |
| `ConvertMidi1Message(timestamp, group, statusByte, dataByte1, dataByte2)` | Turns a MIDI 1.0 message with two data bytes into a `MidiMessage32` |

## WinRT MIDI 1.0 system messages

| Static Method | Description |
| --------------- | ----------- |
| `ConvertMidi1TimeCodeMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiTimeCodeMessage` into a `MidiMessage32` |
| `ConvertMidi1SongPositionPointerMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiSongPositionPointerMessage` into a `MidiMessage32` |
| `ConvertMidi1SongSelectMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiSongSelectMessage` into a `MidiMessage32` |
| `ConvertMidi1TuneRequestMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiTuneRequestMessage` into a `MidiMessage32` |
| `ConvertMidi1TimingClockMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiTimingClockMessage` into a `MidiMessage32` |
| `ConvertMidi1StartMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiStartMessage` into a `MidiMessage32` |
| `ConvertMidi1ContinueMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiContinueMessage` into a `MidiMessage32` |
| `ConvertMidi1StopMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiStopMessage` into a `MidiMessage32` |
| `ConvertMidi1ActiveSensingMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiActiveSensingMessage` into a `MidiMessage32` |
| `ConvertMidi1SystemResetMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiSystemResetMessage` into a `MidiMessage32` |

## WinRT MIDI 1.0 channel voice messages

| Static Method | Description |
| --------------- | ----------- |
| `ConvertMidi1ChannelPressureMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiChannelPressureMessage` into a `MidiMessage32` |
| `ConvertMidi1NoteOffMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiNoteOffMessage` into a `MidiMessage32` |
| `ConvertMidi1NoteOnMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiNoteOnMessage` into a `MidiMessage32` |
| `ConvertMidi1PitchBendChangeMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiPitchBendChangeMessage` into a `MidiMessage32` |
| `ConvertMidi1PolyphonicKeyPressureMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiPolyphonicKeyPressureMessage` into a `MidiMessage32` |
| `ConvertMidi1ProgramChangeMessage(timestamp, group, originalMessage)` | Turns a WinRT MIDI 1.0 `MidiProgramChangeMessage` into a `MidiMessage32` |

## Byte and word lists

| Static Method | Description |
| ------------- | ----------- |
| `ConvertMidi1MessageToUmpWords(group, originalMessage)` | Turns a WinRT MIDI 1.0 `IMidiMessage` into a list of UMP words for this group |
| `ConvertMidi1CompleteMessageBytesToUmpWords(group, midi1Bytes, allowRunningStatus)` | Turns a list of raw MIDI 1.0 bytes into UMP words. Set `allowRunningStatus` to `true` to handle running status |
| `ConvertMidi1CompleteMessageBytesToUmpWords(group, midi1Bytes, allowRunningStatus, converterState)` | The same, but uses a [`MidiBytestreamToUmpMessageConverterState`]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiBytestreamToUmpMessageConverterState/) to remember partial messages and running status between calls on the same stream |
| `ConvertSingleGroupCompleteMessageUmpWordsToMidi1Bytes(umpWords)` | Turns UMP words, all from one group, back into MIDI 1.0 bytes |
| `ConvertHexByteStringToByteArray(hexByteString)` | Turns a string of hex bytes, with or without spaces (such as `"F0 41 10 F7"`), into a byte array. Handy for reading SysEx that someone typed in |

## Samples

Use this class when your app already has MIDI 1.0 bytestream data, which is usually the case when you move an app from WinMM.

* [C++/WinRT sysex-send-bytes](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/sysex-send-bytes)
* [C# sysex-send-bytes](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/sysex-send-bytes)
