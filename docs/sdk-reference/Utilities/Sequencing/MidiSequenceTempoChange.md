---
layout: sdk_reference_page
title: MidiSequenceTempoChange
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: struct
description: One tempo change in a sequence, in both the file's terms and in beats per minute
---

`MidiSequenceTempoChange` is one entry in the tempo map of a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/). Get them from the sequence's `TempoMap` collection, which is sparse enough to hand over whole.

## Struct Fields

| Field | Description |
| ----- | ----------- |
| `Tick` | Where the change takes effect |
| `MicrosecondsPerQuarterNote` | The tempo as the file stores it |
| `BeatsPerMinute` | The same tempo, derived, as quarter notes per minute |
| `MicrosecondsAtTick` | Where this tick falls in time, so a position lookup does not have to walk the map |

## Remarks

A file stores tempo as microseconds per quarter note and never as a beats per minute value, so `BeatsPerMinute` here is derived.

It is also **always quarter notes per minute regardless of the meter in force**, which is what the file format defines and what a sequencer transport shows. In a compound meter such as 6/8 the conducted pulse is a dotted quarter, so this number is exact but is not the felt tempo. If your application shows a conductor's tempo rather than a transport tempo, convert it yourself using the [`MidiSequenceTimeSignature`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimeSignature/) in force.
