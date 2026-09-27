---
layout: sdk_reference_page
title: MidiFileWriteOptions
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: Choices about how a sequence is written out as a Standard MIDI File
---

`MidiFileWriteOptions` holds the few choices [`MidiStandardFileWriter`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileWriter/) offers. The defaults make the file most apps want, so you only need this type when one of them isn't what you want.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiFileWriteOptions()` | Creates options with the defaults |

## Properties

| Property | Description |
| -------- | ----------- |
| `WriteSingleTrack` | Merges every track into one and writes a format 0 file. You lose which track each message came from. A sequence with one track is written as format 0 either way |
| `UseRunningStatus` | Leaves the status byte off a message that has the same status as the one before it |
| `AbsoluteTimingTicksPerQuarterNote` | The ticks per quarter note to use when the sequence has absolute timing. Ignored for a sequence with musical timing, which keeps its own ticks per quarter note |
| `MaximumFileBytes` | The largest file this is allowed to make |

## Remarks

**`UseRunningStatus` is false by default.** Every reader understands running status, and it makes a file with lots of notes noticeably smaller. But a file written without it handles a damaged byte better, because each message says what it is.

**`WriteSingleTrack` is what you want for a recording.** A recording of what one device sent has no useful tracks, and a single track is easier to drop into a sequencer.

**`AbsoluteTimingTicksPerQuarterNote` only matters for a sequence you built yourself.** A Standard MIDI File has no way to say "a tick is a microsecond," so a sequence with absolute timing is laid out at 120 beats per minute with this many ticks per quarter note. At the default of 960, one tick is a little over half a millisecond. Raise it if you need finer timing and the app reading the file can handle it. The format allows up to 32767.

**Going over `MaximumFileBytes` doesn't crash and doesn't throw an exception.** Nothing is written, and [`MidiFileWriteResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteResult/) reports `TooLarge`.
