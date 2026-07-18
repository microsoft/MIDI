---
layout: sdk_reference_page
title: MidiMessage32
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable, Windows.Devices.Midi2.IMidiUniversalPacket
description: Represents a one-word (32-bit) UMP message
---

`MidiMessage32` is used for short messages such as utility messages and MIDI 1.0 messages in UMP format.

## Properties and Methods

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| `Word0` | First 32-bit MIDI word|

| Function | Description |
| -------- | ----------- |
| `MidiMessage32()` | Default constructor |
| `MidiMessage32(timestamp, word0)` | Construct a new message with a timestamp and 32-bit MIDI word |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateFromStruct(timestamp, message)` | Creates a `MidiMessage32` from a `MidiMessageStruct` and a timestamp |
