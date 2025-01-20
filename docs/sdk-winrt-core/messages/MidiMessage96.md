---
layout: api_page
title: MidiMessage96
parent: Messages
grand_parent: Midi2 core
has_children: false
---

# MidiMessage96

`MidiMessage96` is currently unused in the MIDI 2.0 UMP specification.

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
| `Word0` | First 32-bit MIDI word |
| `Word1` | Second 32-bit MIDI word |
| `Word2` | Third 32-bit MIDI word |

| Function | Description |
| -------- | ----------- |
| `MidiMessage96()` | Default constructor |
| `MidiMessage96(timestamp, word0, word1, word2)` | Construct a new message with a timestamp and all 32 bit MIDI words |

## IDL

[MidiMessage96 IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiMessage96.idl)

