---
layout: api_page
title: Midi2ChannelVoiceMessageStatus
parent: Messages
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# Midi2ChannelVoiceMessageStatus Enumeration

Status to use for MIDI 2.0 Channel Voice messages. These are message type 4 messages.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `RegisteredPerNoteController` | `0x0` | MIDI 2.0 Registered per-note controller message |
| `AssignablePerNoteController` | `0x1` | MIDI 2.0 Assignable per-note controller message |
| `RegisteredController` | `0x2` | MIDI 2.0 Registered controller message |
| `AssignableController` | `0x3` | MIDI 2.0 Assignable controller message |
| `RelativeRegisteredController` | `0x4` | MIDI 2.0 Relative registered controller message |
| `RelativeAssignableController` | `0x5` | MIDI 2.0 Relative assignable controller message |
| `PerNotePitchBend` | `0x6` | MIDI 2.0 per-note pitch bend message |
| `NoteOff` | `0x8` | MIDI 2.0 Note Off message |
| `NoteOn` | `0x9` | MIDI 2.0 Note On message |
| `PolyPressure` | `0xA` | MIDI 2.0 polyphonic pressure message |
| `ControlChange` | `0xB` | MIDI 2.0 control change message |
| `ProgramChange` | `0xC` | MIDI 2.0 program change message |
| `ChannelPressure` | `0xD` | MIDI 2.0 channel pressure message |
| `PitchBend` | `0xE` | MIDI 2.0 pitch bend message |
| `PerNoteManagement` | `0xF` | MIDI 2.0 per-note management message |

## IDL

[Midi2ChannelVoiceMessageStatusEnum IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/Midi2ChannelVoiceMessageStatusEnum.idl)
