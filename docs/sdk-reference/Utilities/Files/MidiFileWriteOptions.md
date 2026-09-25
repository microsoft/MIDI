---
layout: sdk_reference_page
title: MidiFileWriteOptions
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: Choices about how a sequence is written out as a Standard MIDI File
---

`MidiFileWriteOptions` holds the handful of choices [`MidiStandardFileWriter`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileWriter/) offers. The defaults produce the file most applications want, so you only need this type when one of them is not what you are after.

## Properties

| Property | Description |
| -------- | ----------- |
| `WriteSingleTrack` | Merge every track into one and write a format 0 file. Which track a message came from is lost. A sequence with one track is written as format 0 either way |
| `UseRunningStatus` | Leave the status byte off a message which repeats the one before it |
| `AbsoluteTimingTicksPerQuarterNote` | Ticks per quarter note used when the sequence is absolutely timed. Ignored for a musically timed sequence, which keeps its own division |
| `MaximumFileBytes` | Ceiling on the file this is allowed to produce |

## Remarks

**`UseRunningStatus` is false by default.** Every reader handles running status, and it makes a note-heavy file noticeably smaller, but a file written without it survives a damaged byte better because each message says what it is.

**`WriteSingleTrack` is what you want for a capture.** A recording of what one device sent has no useful track structure, and a single track is easier to drop into a sequencer.

**`AbsoluteTimingTicksPerQuarterNote` only matters for a sequence you built yourself.** A Standard MIDI File has no way of saying "a tick is a microsecond", so an absolutely timed sequence is laid out at 120 beats per minute using this division. At the default of 960, one tick is a little over half a millisecond. Raise it if you need finer timing and the application reading the file can cope; the format allows up to 32767.

**Exceeding `MaximumFileBytes` is not a crash and not an exception.** Nothing is written and [`MidiFileWriteResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteResult/) reports `TooLarge`.
