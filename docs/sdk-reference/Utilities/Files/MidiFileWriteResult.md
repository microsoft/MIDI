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
| `SkippedEventCount` | How many messages were left out because MIDI 1.0 can't express them |

## Remarks

`SkippedEventCount` is zero for any sequence that came from a Standard MIDI File, because everything in it is already MIDI 1.0. It only matters when the sequence has Universal MIDI Packets. A per-note controller, a per-note pitch bend, and 8-bit System Exclusive have no MIDI 1.0 form, so there's nothing to write for them.

A count above zero isn't a failure. The rest of the file was written, and `Succeeded` is still true. Whether to tell people depends on what they were saving. For a recording of a MIDI 2.0 device, it's usually worth telling them.
