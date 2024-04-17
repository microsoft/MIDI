---
layout: api_page
title: MidiSystemExclusive8Status
parent: Messages
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiSystemExclusive8Status Enumeration

Used to indicate the type of System Exclusive 8 Universal MIDI Packet (UMP) as per the MIDI 2.0 UMP specification.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `CompleteMessageInSingleMessagePacket` | `0x0` |  |
| `StartMessagePacket` | `0x1` |  |
| `ContinueMessagePacket` | `0x2` |  |
| `EndMessagePacket` | `0x3` |  |

## IDL

[MidiSystemExclusive8StatusEnum IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiSystemExclusive8StatusEnum.idl)
