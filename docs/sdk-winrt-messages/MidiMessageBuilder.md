---
layout: api_page
title: MidiMessageBuilder
parent: Midi2.Messages
has_children: false
---

# MidiMessageBuilder

This class includes static functions to assist in constructing common message types.

## Static Functions

| Function | Description |
| --------------- | ----------- |
| `BuildUtilityMessage` | Returns a `MidiMessage32` with the provided data. |
| `BuildSystemMessage` | Returns a `MidiMessage32` with the provided data. |
| `BuildMidi1ChannelVoiceMessage` | Returns a `MidiMessage32` with the provided data. |
| `BuildSystemExclusive7Message` | Returns a `MidiMessage64` with the provided data. |
| `BuildMidi2ChannelVoiceMessage` | Returns a `MidiMessage64` with the provided data. |
| `BuildSystemExclusive8Message` | Returns a `MidiMessage128` with the provided data. |
| `BuildMixedDataSetChunkHeaderMessage` | Returns a `MidiMessage128` with the provided data. |
| `BuildMixedDataSetChunkDataMessage` | Returns a `MidiMessage128` with the provided data. |
| `BuildFlexDataMessage` | Returns a `MidiMessage128` with the provided data. |
| `BuildStreamMessage` | Returns a `MidiMessage128` with the provided data. |

## IDL

[MidiMessageBuilder IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-messages/MidiMessageBuilder.idl)
