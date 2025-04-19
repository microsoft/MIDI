---
layout: sdk_reference_page
title: MidiPacketType
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: Indicates the number of words in a MIDI message
---

## Properties

The values correspond to the number of 32-bit MIDI words in the packet.

| Property | Value | Description |
| -------- | ------- | ------ |
| `UnknownOrInvalid` | `0` | An invalid zero-length Universal MIDI Packet |
| `UniversalMidiPacket32` | `1` | 32-bit (1 word) Universal MIDI Packet |
| `UniversalMidiPacket64` | `2` | 64-bit (2 words) Universal MIDI Packet |
| `UniversalMidiPacket96` | `3` | 96-bit (3 words) Universal MIDI Packet |
| `UniversalMidiPacket128` | `4` | 128-bit (4 words) Universal MIDI Packet |
