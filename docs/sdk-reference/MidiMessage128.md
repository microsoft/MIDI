---
layout: sdk_reference_page
title: MidiMessage128
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Windows.Foundation.IStringable, Microsoft.Windows.Devices.Midi2.IMidiUniversalPacket
description: Represents a four-word (128-bit) UMP message
---

`MidiMessage128` is used for some data messages as well as important "Type F" stream metadata messages.

## Properties and Methods

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| `Word0` | First 32-bit MIDI word |
| `Word1` | Second 32-bit MIDI word |
| `Word2` | Third 32-bit MIDI word |
| `Word3` | Fourth 32-bit MIDI word |

| Function | Description |
| -------- | ----------- |
| `MidiMessage128()` | Default constructor |
| `MidiMessage128(timestamp, word0, word1, word2, word3)` | Construct a new message with a timestamp and all 32 bit MIDI words |
