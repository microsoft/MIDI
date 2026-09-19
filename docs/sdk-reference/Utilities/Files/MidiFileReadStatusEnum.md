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
| `NotAMidiFile` | `1` | The header is not one we recognize |
| `UnsupportedFormat` | `2` | The file is a MIDI file, but of a kind this reader does not handle |
| `CorruptData` | `3` | The file could not be read far enough to produce anything playable |
| `NoPlayableData` | `4` | The file read cleanly, but there is nothing in it to send |
| `TooLarge` | `5` | Refused before reading, against the limits in [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) |
| `ReadError` | `6` | The stream or file could not be read |

## Remarks

`Success` does not mean the whole file was read. A file which stops making sense partway through still reports `Success`, with `Truncated` set to true on the result, because what was read is usually still worth hearing.
