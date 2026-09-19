---
layout: sdk_reference_page
title: MidiFileReadResult
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: The outcome of reading a MIDI file, and the sequence it produced
---

`MidiFileReadResult` is what every read method on [`MidiStandardFileReader`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileReader/) returns. Check `Succeeded` before reading `Sequence`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Succeeded` | True when the file produced a usable sequence |
| `Status` | The [`MidiFileReadStatus`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadStatusEnum/) describing what happened |
| `Truncated` | True when the file ran out, or stopped making sense, before it said it would. What was read is still in the sequence and is still playable |
| `Sequence` | The [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) which was read. Null unless `Succeeded` is true |
| `DeclaredTrackCount` | The number of tracks the file header said it contained |
| `ReadTrackCount` | The number of tracks actually read |

## Remarks

`Succeeded` and `Truncated` can both be true at the same time, and that is the common case for a damaged file. A truncated read is still worth playing, so this is reported rather than treated as a failure.

A file which declares more tracks than it contains is common enough to be worth reporting rather than hiding, which is why `DeclaredTrackCount` and `ReadTrackCount` are both here. If they differ, the file is not what it claimed to be.
