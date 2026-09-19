---
layout: sdk_reference_page
title: MidiFileReadOptions
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: Limits applied while reading a MIDI file, so a hostile or broken file cannot ask for an unreasonable amount of memory
---

A MIDI file usually arrives from somewhere your application does not control, so nothing is allocated from a length the file declares without first checking that the bytes are really there. `MidiFileReadOptions` holds the ceilings which stop a hostile or broken file from asking for an unreasonable amount of memory.

The defaults are generous enough for real music, so you only need this type when you want to be stricter, or when you are reading something unusually large on purpose.

## Properties

| Property | Description |
| -------- | ----------- |
| `MaximumFileBytes` | Largest file which will be read at all. A file larger than this is refused before any parsing happens |
| `MaximumTrackCount` | Largest number of tracks which will be read |
| `MaximumEventCount` | Largest number of events which will be read |
| `MaximumTextEventCount` | Largest number of text and lyric events which will be read |
| `MaximumSingleMessageBytes` | Largest single message, which in practice means the largest System Exclusive message |
| `FailOnUnreadableData` | When true, a file which stops making sense partway through is refused outright instead of being read up to that point and reported as truncated |

## Remarks

Exceeding a limit is not a crash and not an exception. The read stops and [`MidiFileReadResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadResult/) reports it, with `Status` set to `TooLarge` when the file was refused before reading.

`FailOnUnreadableData` is false by default. A file with a damaged tail is usually still worth hearing, so the default is to keep what was read and set `Truncated` on the result.
