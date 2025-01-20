---
layout: api_page
title: MidiMessage32
parent: Messages
grand_parent: Midi2 core
has_children: false
---

# MidiMessage32

`MidiMessage32` is used for short messages such as utility messages and MIDI 1.0 messages in UMP format.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Implements

`Microsoft.Windows.Devices.Midi2.IMidiUniversalPacket`
`Windows.Foundation.IStringable`

## Properties and Methods

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| `Word0` | First 32-bit MIDI word|

| Function | Description |
| -------- | ----------- |
| `MidiMessage32()` | Default constructor |
| `MidiMessage32(timestamp, word0)` | Construct a new message with a timestamp and 32 bit MIDI word |

## IDL

[MidiMessage32 IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiMessage32.idl)
