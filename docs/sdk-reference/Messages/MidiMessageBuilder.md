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
