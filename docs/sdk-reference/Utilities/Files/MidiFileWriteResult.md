---
layout: sdk_reference_page
title: MidiFileWriteResult
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: The outcome of writing a sequence out as a Standard MIDI File
---

`MidiFileWriteResult` is what every write method on [`MidiStandardFileWriter`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileWriter/) returns. Check `Succeeded` before telling anyone the file was saved.

## Properties

| Property | Description |
| -------- | ----------- |
| `Succeeded` | True when the file was written |
| `Status` | The [`MidiFileWriteStatus`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteStatusEnum/) describing what happened |
| `TrackCount` | How many tracks were written |
| `ByteCount` | How large the file is |
| `SkippedEventCount` | How many messages were left out because MIDI 1.0 has no way to express them |

## Remarks

`SkippedEventCount` is zero for any sequence which came from a Standard MIDI File, because everything in such a sequence is already MIDI 1.0. It only becomes interesting when the sequence holds Universal MIDI Packets: a per-note controller, a per-note pitch bend and 8-bit system exclusive have no MIDI 1.0 form, so there is nothing to write for them.

A non-zero count is not a failure. The rest of the file was written, and `Succeeded` is still true. Whether it is worth telling the customer depends on what they were saving; for a capture of a MIDI 2.0 device it usually is.
