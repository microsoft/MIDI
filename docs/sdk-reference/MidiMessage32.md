---
layout: sdk_reference_page
title: MidiMessage32
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable, Windows.Devices.Midi2.IMidiUniversalPacket
description: Represents a one-word (32-bit) UMP message
---

`MidiMessage32` holds a one-word (32-bit) Universal MIDI Packet. Utility messages, system messages, and MIDI 1.0 channel voice messages are all this size.

## Properties and Methods

It has everything in [`IMidiUniversalPacket`]({{ site.baseurl }}/sdk-reference/IMidiUniversalPacket/), plus:

| Property | Description |
| -------- | ----------- |
| `Word0` | The 32-bit word that holds the whole message |

| Constructor | Description |
| -------- | ----------- |
| `MidiMessage32()` | Creates an empty message |
| `MidiMessage32(timestamp, word0)` | Creates a message with this timestamp and word |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateFromStruct(timestamp, message)` | Creates a `MidiMessage32` from a `MidiMessageStruct`, with this timestamp |
