---
layout: sdk_reference_page
title: Midi1ChannelVoiceMessageStatus
namespace: Microsoft.Windows.Devices.Midi2.Messages
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: MIDI 1.0 channel voice message status values
---

Status to use for MIDI 1.0 Channel Voice messages. Note that not all MIDI 1.0 messages are channel voice messages, so this is not an exhaustive list of MIDI 1.0 messages. However, this is the total set of MIDI 1.0 messages which can be used in a MIDI Universal MIDI Packet Message type 2.

## Values

| Property | Value | Description |
| -------- | ------- | ------ |
| `NoteOff` | `0x8` | MIDI 1.0 Note Off message |
| `NoteOn` | `0x9` | MIDI 1.0 Note On message |
| `PolyPressure` | `0xA` | MIDI 1.0 polyphonic pressure message |
| `ControlChange` | `0xB` | MIDI 1.0 control change message |
| `ProgramChange` | `0xC` | MIDI 1.0 program change message |
| `ChannelPressure` | `0xD` | MIDI 1.0 channel pressure message |
| `PitchBend` | `0xE` | MIDI 1.0 pitch bend message |
