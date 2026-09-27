---
layout: sdk_reference_page
title: MidiFileReadOptions
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: Limits applied while reading a MIDI file, so a hostile or broken file cannot ask for an unreasonable amount of memory
---

A MIDI file usually comes from somewhere your app doesn't control. So the reader never sets aside memory for a length the file claims, until it has checked that the bytes are really there. `MidiFileReadOptions` holds the limits that stop a harmful or broken file from asking for too much memory.

The defaults are big enough for real music, so you only need this type when you want to be stricter, or when you're reading something unusually large on purpose.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiFileReadOptions()` | Creates options with the default limits |

## Properties

| Property | Description |
| -------- | ----------- |
| `MaximumFileBytes` | The largest file that will be read at all. A larger file is refused before any reading starts |
| `MaximumTrackCount` | The most tracks that will be read |
| `MaximumEventCount` | The most events that will be read |
| `MaximumTextEventCount` | The most text and lyric events that will be read |
| `MaximumSingleMessageBytes` | The largest single message. In practice, that means the largest System Exclusive message |
| `FailOnUnreadableData` | When true, a file that stops making sense partway through is refused completely, instead of being read up to that point and reported as truncated |

## Remarks

Going over a limit doesn't crash and doesn't throw an exception. The read stops, and [`MidiFileReadResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadResult/) reports it, with `Status` set to `TooLarge` when the file was refused before reading.

`FailOnUnreadableData` is false by default. A file with a damaged end is usually still worth hearing, so by default the reader keeps what it read and sets `Truncated` on the result.
