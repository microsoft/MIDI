---
layout: sdk_reference_page
title: MidiMessage64
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Windows.Foundation.IStringable, Microsoft.Windows.Devices.Midi2.IMidiUniversalPacket
description: Represents a two-word (64-bit) UMP message
---

`MidiMessage64` is used for some data messages and for MIDI 2.0 Channel Voice messages.

## Properties and Methods

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| `Word0` | First 32-bit MIDI word|
| `Word1` | Second 32-bit MIDI word |

| Function | Description |
| -------- | ----------- |
| `MidiMessage64()` | Default constructor |
| `MidiMessage64 (timestamp, word0, word1)` | Construct a new message with a timestamp and all 32 bit MIDI words |
