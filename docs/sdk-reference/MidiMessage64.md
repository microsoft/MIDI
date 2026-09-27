---
layout: sdk_reference_page
title: MidiMessage64
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable, Windows.Devices.Midi2.IMidiUniversalPacket
description: Represents a two-word (64-bit) UMP message
---

`MidiMessage64` holds a two-word (64-bit) Universal MIDI Packet. MIDI 2.0 channel voice messages and System Exclusive 7 messages are this size.

## Properties and Methods

It has everything in [`IMidiUniversalPacket`]({{ site.baseurl }}/sdk-reference/IMidiUniversalPacket/), plus:

| Property | Description |
| -------- | ----------- |
| `Word0` | The first 32-bit word |
| `Word1` | The second 32-bit word |

| Constructor | Description |
| -------- | ----------- |
| `MidiMessage64()` | Creates an empty message |
| `MidiMessage64(timestamp, word0, word1)` | Creates a message with this timestamp and both words |
| `MidiMessage64(timestamp, words)` | Creates a message with this timestamp from an array of 32-bit words |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateFromStruct(timestamp, message)` | Creates a `MidiMessage64` from a `MidiMessageStruct`, with this timestamp |
