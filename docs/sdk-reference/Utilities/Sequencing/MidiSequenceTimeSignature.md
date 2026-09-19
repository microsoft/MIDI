---
layout: sdk_reference_page
title: MidiSequenceTimeSignature
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: struct
description: One time signature change in a sequence
---

`MidiSequenceTimeSignature` is one entry in the time signature map of a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/). Get them from the sequence's `TimeSignatureMap` collection.

## Struct Fields

| Field | Description |
| ----- | ----------- |
| `Tick` | Where the change takes effect |
| `Numerator` | Beats per bar |
| `Denominator` | The note value which gets the beat. 4 means a quarter note, 8 an eighth, and so on |
| `TicksPerBar` | How long a bar is under this signature |
| `BarNumberAtTick` | The bar number at `Tick`, counted from one |

## Remarks

`TicksPerBar` and `BarNumberAtTick` are worked out while reading, so asking where a tick falls in the music does not mean walking the file. That is also what makes `GetBarPositionAtTick` on the sequence cheap enough to call while drawing.
