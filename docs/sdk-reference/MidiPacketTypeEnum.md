---
layout: sdk_reference_page
title: MidiPacketType
namespace: Windows.Devices.Midi2
type: enum
description: Indicates the number of words in a MIDI message
---

## Properties

The values correspond to the number of 32-bit MIDI words in the packet.

| Property | Value | Description |
| -------- | ------- | ------ |
| `UnknownOrInvalid` | `0x00000000` | An invalid zero-length Universal MIDI Packet |
| `UniversalMidiPacket32` | `0x00000001` | 32-bit (1 word) Universal MIDI Packet |
| `UniversalMidiPacket64` | `0x00000002` | 64-bit (2 words) Universal MIDI Packet |
| `UniversalMidiPacket96` | `0x00000003` | 96-bit (3 words) Universal MIDI Packet |
| `UniversalMidiPacket128` | `0x00000004` | 128-bit (4 words) Universal MIDI Packet |
