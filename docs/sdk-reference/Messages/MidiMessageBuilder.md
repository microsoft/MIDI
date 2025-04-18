---
layout: sdk_reference_page
title: MidiMessageBuilder
namespace: Microsoft.Windows.Devices.Midi2.Messages
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Helper class to construct common message types
---

This class includes static functions to assist in constructing common message types.

## Static Functions

| Function | Description |
| --------------- | ----------- |
| `BuildUtilityMessage` | Returns a type 0 `MidiMessage32` with the provided data. |
| `BuildSystemMessage` | Returns a type 1 `MidiMessage32` with the provided data. |
| `BuildMidi1ChannelVoiceMessage` | Returns a type 2 `MidiMessage32` with the provided data. |
| `BuildSystemExclusive7Message` | Returns a type 3 `MidiMessage64` with the provided data. |
| `BuildMidi2ChannelVoiceMessage` | Returns a type 4`MidiMessage64` with the provided data. |
| `BuildSystemExclusive8Message` | Returns a type 5 `MidiMessage128` with the provided data. |
| `BuildMixedDataSetChunkHeaderMessage` | Returns a Type 5 `MidiMessage128` with the provided data. |
| `BuildMixedDataSetChunkDataMessage` | Returns a Type 5 `MidiMessage128` with the provided data. |
| `BuildFlexDataMessage` | Returns a type 0xD `MidiMessage128` with the provided data. |
| `BuildStreamMessage` | Returns a type 0xF `MidiMessage128` with the provided data. |
