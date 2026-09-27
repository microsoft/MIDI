---
layout: sdk_reference_page
title: Midi1ChannelVoiceMessageStatus
namespace: Windows.Devices.Midi2.Utilities.Messages
type: enum
description: MIDI 1.0 channel voice message status values
---

The status values for MIDI 1.0 channel voice messages. Not every MIDI 1.0 message is a channel voice message, so this isn't a full list of MIDI 1.0 messages. But it is every MIDI 1.0 message that can go in a Universal MIDI Packet of message type 2.

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
