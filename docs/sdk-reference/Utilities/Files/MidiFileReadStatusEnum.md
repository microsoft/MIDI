---
layout: sdk_reference_page
title: MidiFileReadStatus
namespace: Windows.Devices.Midi2.Utilities.Files
type: enum
description: Describes the outcome of reading a MIDI file
---

`MidiFileReadStatus` is reported by [`MidiFileReadResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadResult/) and tells you why a read did or did not produce a sequence.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Success` | `0` | The file produced a usable sequence. Check `Truncated` on the result to find out whether all of it was read |
| `NotAMidiFile` | `1` | The header isn't one this reader recognizes |
| `UnsupportedFormat` | `2` | It's a MIDI file, but a kind this reader doesn't handle |
| `CorruptData` | `3` | The file couldn't be read far enough to get anything playable |
| `NoPlayableData` | `4` | The file read cleanly, but there's nothing in it to send |
| `TooLarge` | `5` | Refused before reading, because of the limits in [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) |
| `ReadError` | `6` | The stream or file couldn't be read |

## Remarks

`Success` doesn't mean the whole file was read. A file that stops making sense partway through still reports `Success`, with `Truncated` set to true on the result, because what was read is usually still worth hearing.
