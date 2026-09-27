---
layout: sdk_reference_page
title: MidiSequenceNote
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: struct
description: A note with both ends already paired
---

`MidiSequenceNote` is a note with its start and end already matched up, so you don't have to pair note ons with note offs yourself.

This is a struct, not a class, because a display asks for every note it can see on every frame, and a file can hold hundreds of millions of them. As a struct, a whole window of notes can be copied to your app in one call.

## Struct Fields

| Field | Description |
| ----- | ----------- |
| `StartTick` | Where the note begins |
| `EndTick` | Where the note ends |
| `TrackIndex` | The track the note is on |
| `ChannelIndex` | The channel index, starting at zero |
| `NoteNumber` | The MIDI note number |
| `Velocity` | The note on velocity |

## Remarks

Get these by calling [`MidiSequence.FillNotesInTickRange`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) with an array you own. Use `GetNoteCountInTickRange` to know how big to make it.

Notes are sorted by where they start, so a note that began before your window can still be sounding inside it. Subtract the sequence's `LongestNoteTicks` from the start of your window to catch those.
