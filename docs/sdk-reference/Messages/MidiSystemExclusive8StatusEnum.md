---
layout: sdk_reference_page
title: MidiSystemExclusive8Status
namespace: Microsoft.Windows.Devices.Midi2.Messages
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: Indicates the type of System Exclusive 8-bit message
---

Used to indicate the type of System Exclusive 8 Universal MIDI Packet (UMP) as per the MIDI 2.0 UMP specification.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `CompleteMessageInSingleMessagePacket` | `0x0` | The entire SysEx message is in this one packet |
| `StartMessagePacket` | `0x1` | Start of a multi-message SysEx stream that consists of at least 2 messages |
| `ContinueMessagePacket` | `0x2` | Continue a multi-message SysEx stream that consists of at least 3 messages |
| `EndMessagePacket` | `0x3` | Complete a multi-message SysEx stream |
