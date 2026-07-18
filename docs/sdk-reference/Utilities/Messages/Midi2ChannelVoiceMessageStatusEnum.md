---
layout: sdk_reference_page
title: Midi2ChannelVoiceMessageStatus
namespace: Windows.Devices.Midi2.Utilities.Messages
type: enum
description: MIDI 2.0 channel voice message status values
---

Status to use for MIDI 2.0 Channel Voice messages. These are message type 4 messages.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `RegisteredPerNoteController` | `0x00000000` | MIDI 2.0 Registered per-note controller message |
| `AssignablePerNoteController` | `0x00000001` | MIDI 2.0 Assignable per-note controller message |
| `RegisteredController` | `0x00000002` | MIDI 2.0 Registered controller message |
| `AssignableController` | `0x00000003` | MIDI 2.0 Assignable controller message |
| `RelativeRegisteredController` | `0x00000004` | MIDI 2.0 Relative registered controller message |
| `RelativeAssignableController` | `0x00000005` | MIDI 2.0 Relative assignable controller message |
| `PerNotePitchBend` | `0x00000006` | MIDI 2.0 per-note pitch bend message |
| `NoteOff` | `0x00000008` | MIDI 2.0 Note Off message |
| `NoteOn` | `0x00000009` | MIDI 2.0 Note On message |
| `PolyPressure` | `0x0000000A` | MIDI 2.0 polyphonic pressure message |
| `ControlChange` | `0x0000000B` | MIDI 2.0 control change message |
| `ProgramChange` | `0x0000000C` | MIDI 2.0 program change message |
| `ChannelPressure` | `0x0000000D` | MIDI 2.0 channel pressure message |
| `PitchBend` | `0x0000000E` | MIDI 2.0 pitch bend message |
| `PerNoteManagement` | `0x0000000F` | MIDI 2.0 per-note management message |
