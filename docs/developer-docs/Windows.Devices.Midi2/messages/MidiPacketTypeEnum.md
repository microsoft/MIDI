---
layout: api_page
title: MidiPacketType
parent: Messages
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiPacketType Enumeration

The values correspond to the number of 32-bit MIDI words in the packet.

| Property | Value | Description |
| -------- | ------- | ------ |
| `UnknownOrInvalid` | `0` | An invalid zero-length Universal MIDI Packet |
| `UniversalMidiPacket32` | `1` | 32-bit (1 word) Universal MIDI Packet |
| `UniversalMidiPacket64` | `2` | 64-bit (2 words) Universal MIDI Packet |
| `UniversalMidiPacket96` | `3` | 96-bit (3 words) Universal MIDI Packet |
| `UniversalMidiPacket128` | `4` | 128-bit (4 words) Universal MIDI Packet |

## IDL

[MidiPacketType IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiPacketTypeEnum.idl)
