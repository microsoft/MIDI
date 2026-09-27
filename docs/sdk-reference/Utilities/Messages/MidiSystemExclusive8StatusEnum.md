---
layout: sdk_reference_page
title: MidiSystemExclusive8Status
namespace: Windows.Devices.Midi2.Utilities.Messages
type: enum
description: Indicates the type of System Exclusive 8-bit message
---

Says which part of a System Exclusive 8 message a Universal MIDI Packet (UMP) holds, as the MIDI 2.0 UMP specification defines.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `CompleteMessageInSingleMessagePacket` | `0x00000000` | The whole SysEx message is in this one packet |
| `StartMessagePacket` | `0x00000001` | The start of a SysEx message that takes at least 2 packets |
| `ContinueMessagePacket` | `0x00000002` | A middle part of a SysEx message that takes at least 3 packets |
| `EndMessagePacket` | `0x00000003` | The end of a SysEx message that takes more than one packet |
