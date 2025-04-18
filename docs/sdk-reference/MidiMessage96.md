---
layout: sdk_reference_page
title: MidiMessage96
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Windows.Foundation.IStringable, Microsoft.Windows.Devices.Midi2.IMidiUniversalPacket
description: Represents a three-word (96-bit) UMP message
---

`MidiMessage96` is currently unused in the MIDI 2.0 UMP specification.

## Properties and Methods

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| `Word0` | First 32-bit MIDI word |
| `Word1` | Second 32-bit MIDI word |
| `Word2` | Third 32-bit MIDI word |

| Function | Description |
| -------- | ----------- |
| `MidiMessage96()` | Default constructor |
| `MidiMessage96 (timestamp, word0, word1, word2)` | Construct a new message with a timestamp and all 32 bit MIDI words |
