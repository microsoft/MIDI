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
| `NothingToWrite` | `1` | The sequence is empty, so there'd be no file worth keeping |
| `TooLarge` | `2` | The file would be larger than `MaximumFileBytes` in [`MidiFileWriteOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteOptions/). Nothing was written |
| `AccessDenied` | `3` | The file couldn't be opened for writing |
| `WriteError` | `4` | The stream or file couldn't be written |
| `OutOfMemory` | `5` | The sequence is too big to lay out in memory |

## Remarks

It's worth handling `NothingToWrite` on its own in your app. It usually means someone asked to save before there was anything to save. "There's nothing here yet" is more helpful to tell them than "the file couldn't be written."
