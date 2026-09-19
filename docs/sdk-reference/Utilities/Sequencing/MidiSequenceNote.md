---
layout: sdk_reference_page
title: MidiSequenceNote
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: struct
description: A note with both ends already paired
---

`MidiSequenceNote` is a note with both ends already paired, so you do not have to match note ons to note offs yourself.

This is a struct, not a runtime class, because a display asks for every note in a visible window on every frame, and a file can hold hundreds of millions of them. As a struct, a whole window crosses the ABI boundary in one call.

## Struct Fields

| Field | Description |
| ----- | ----------- |
| `StartTick` | Where the note begins |
| `EndTick` | Where the note ends |
| `TrackIndex` | The track the note is on |
| `ChannelIndex` | The channel index, zero-based |
| `NoteNumber` | The MIDI note number |
| `Velocity` | The note-on velocity |

## Remarks

Fill these from [`MidiSequence.FillNotesInTickRange`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) into an array you own, and size it using `GetNoteCountInTickRange`.

Notes are ordered by where they start, so a note which began before your window can still be sounding inside it. Subtract the sequence's `LongestNoteTicks` from your window start to catch those.
