---
layout: sdk_reference_page
title: MidiMessage128
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable, Windows.Devices.Midi2.IMidiUniversalPacket
description: Represents a four-word (128-bit) UMP message
---

`MidiMessage128` holds a four-word (128-bit) Universal MIDI Packet. System Exclusive 8, Mixed Data Set, Flex Data, and stream messages are this size. Stream messages (message type F) are how endpoints describe themselves, so this size matters a lot.

## Properties and Methods

It has everything in [`IMidiUniversalPacket`]({{ site.baseurl }}/sdk-reference/IMidiUniversalPacket/), plus:

| Property | Description |
| -------- | ----------- |
| `Word0` | The first 32-bit word |
| `Word1` | The second 32-bit word |
| `Word2` | The third 32-bit word |
| `Word3` | The fourth 32-bit word |

| Constructor | Description |
| -------- | ----------- |
| `MidiMessage128()` | Creates an empty message |
| `MidiMessage128(timestamp, word0, word1, word2, word3)` | Creates a message with this timestamp and all four words |
| `MidiMessage128(timestamp, words)` | Creates a message with this timestamp from an array of 32-bit words |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateFromStruct(timestamp, message)` | Creates a `MidiMessage128` from a `MidiMessageStruct`, with this timestamp |
