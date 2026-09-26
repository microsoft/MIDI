---
layout: sdk_reference_page
title: MidiFileWriteStatus
namespace: Windows.Devices.Midi2.Utilities.Files
type: enum
description: Describes the outcome of writing a Standard MIDI File
---

`MidiFileWriteStatus` is reported by [`MidiFileWriteResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteResult/) and tells you why a write did or did not produce a file.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Success` | `0` | The file was written |
| `NothingToWrite` | `1` | The sequence is empty, so there would be no file worth keeping |
| `TooLarge` | `2` | The file would be larger than `MaximumFileBytes` in [`MidiFileWriteOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteOptions/). Nothing was written |
| `AccessDenied` | `3` | The file could not be opened for writing |
| `WriteError` | `4` | The stream or file could not be written |
| `OutOfMemory` | `5` | The sequence is too large to lay out in memory |

## Remarks

`NothingToWrite` is worth handling separately in your interface. It usually means the customer asked to save before there was anything to save, and "there is nothing here yet" is a more useful thing to tell them than "the file could not be written".
