---
layout: sdk_reference_page
title: Midi1ChannelVoiceMessageStatus
namespace: Windows.Devices.Midi2.Utilities.Messages
type: enum
description: MIDI 1.0 channel voice message status values
---

Status to use for MIDI 1.0 Channel Voice messages. Note that not all MIDI 1.0 messages are channel voice messages, so this is not an exhaustive list of MIDI 1.0 messages. However, this is the total set of MIDI 1.0 messages which can be used in a MIDI Universal MIDI Packet Message type 2.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `NoteOff` | `0x00000008` | MIDI 1.0 Note Off message |
| `NoteOn` | `0x00000009` | MIDI 1.0 Note On message |
| `PolyPressure` | `0x0000000A` | MIDI 1.0 polyphonic pressure message |
| `ControlChange` | `0x0000000B` | MIDI 1.0 control change message |
| `ProgramChange` | `0x0000000C` | MIDI 1.0 program change message |
| `ChannelPressure` | `0x0000000D` | MIDI 1.0 channel pressure message |
| `PitchBend` | `0x0000000E` | MIDI 1.0 pitch bend message |
