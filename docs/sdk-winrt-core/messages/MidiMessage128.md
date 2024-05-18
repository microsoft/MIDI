---
layout: api_page
title: MidiMessage128
parent: Messages
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiMessage128

`MidiMessage128` is used for some data messages as well as important "Type F" stream metadata messages.

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

## IDL

[MidiMessage128 IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiMessage128.idl)
