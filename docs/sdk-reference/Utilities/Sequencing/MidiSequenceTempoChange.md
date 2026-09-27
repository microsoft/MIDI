---
layout: sdk_reference_page
title: MidiSequenceTempoChange
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: struct
description: One tempo change in a sequence, in both the file's terms and in beats per minute
---

`MidiSequenceTempoChange` is one entry in the tempo map of a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/). Get them from the sequence's `TempoMap` collection, which is small enough to hand over whole.

## Struct Fields

| Field | Description |
| ----- | ----------- |
| `Tick` | Where the change takes effect |
| `MicrosecondsPerQuarterNote` | The tempo the way the file stores it |
| `BeatsPerMinute` | The same tempo, worked out as quarter notes per minute |
| `MicrosecondsAtTick` | Where this tick falls in time, so looking up a position doesn't have to go through the whole map |

## Remarks

A file stores tempo as microseconds per quarter note, never as beats per minute, so `BeatsPerMinute` here is worked out from it.

It's also **always quarter notes per minute, whatever the time signature**. That's what the file format defines, and what a sequencer's transport shows. In a time signature like 6/8, the beat you'd count or conduct is a dotted quarter note, so this number is correct but isn't the tempo you feel. If your app shows a conductor's tempo instead of a transport tempo, convert it yourself, using the [`MidiSequenceTimeSignature`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimeSignature/) in effect.
