---
layout: sdk_reference_page
title: MidiSequenceTimingMode
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: enum
description: Whether the ticks in a sequence are musical or measured in real time
---

`MidiSequenceTimingMode` says how to read the ticks in a sequence. Set it on a [`MidiSequenceBuilder`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceBuilder/) before you add anything.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Musical` | `0` | Ticks are musical and follow the tempo map. Every Standard MIDI File uses this |
| `Absolute` | `1` | A tick is one microsecond, and tempo doesn't apply |

## Remarks

Use `Musical` for anything that should speed up and slow down with the music. A file read from disk is always musical, and changing the tempo changes when its notes play.

Use `Absolute` when a gap has to be a real gap. Two cases come up often. The first is a step list where you want to wait a set number of milliseconds before the next thing happens, and you don't want that wait to change if the tempo does. The second is a long System Exclusive dump, where a device needs its packets spaced out in real time to keep up.

In `Absolute` mode, a tick is a microsecond, so 250000 means a quarter of a second. Ticks are 32 bits, so a sequence with absolute timing can run for about 71 minutes. That's far longer than the step lists this mode is for, but it's worth knowing before you use it for a whole show.

The mode applies to the whole sequence. A sequence can't be musical in one place and absolute in another, because then the position displays and seeking would have no single answer for where a moment is.
