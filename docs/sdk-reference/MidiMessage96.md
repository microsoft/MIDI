---
layout: sdk_reference_page
title: MidiMessage96
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable, Windows.Devices.Midi2.IMidiUniversalPacket
description: Represents a three-word (96-bit) UMP message
---

`MidiMessage96` holds a three-word (96-bit) Universal MIDI Packet. The UMP specification sets this size aside, but no messages use it yet.

## Properties and Methods

It has everything in [`IMidiUniversalPacket`]({{ site.baseurl }}/sdk-reference/IMidiUniversalPacket/), plus:

| Property | Description |
| -------- | ----------- |
| `Word0` | The first 32-bit word |
| `Word1` | The second 32-bit word |
| `Word2` | The third 32-bit word |

| Constructor | Description |
| -------- | ----------- |
| `MidiMessage96()` | Creates an empty message |
| `MidiMessage96(timestamp, word0, word1, word2)` | Creates a message with this timestamp and all three words |
| `MidiMessage96(timestamp, words)` | Creates a message with this timestamp from an array of 32-bit words |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateFromStruct(timestamp, message)` | Creates a `MidiMessage96` from a `MidiMessageStruct`, with this timestamp |
