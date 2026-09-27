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
| `Truncated` | True when the file ran out, or stopped making sense, before it said it would. What was read is still in the sequence, and it still plays |
| `Sequence` | The [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) that was read. Null unless `Succeeded` is true |
| `DeclaredTrackCount` | How many tracks the file header said it had |
| `ReadTrackCount` | How many tracks were really read |

## Remarks

`Succeeded` and `Truncated` can both be true at the same time. That's the usual case for a damaged file. A truncated file is still worth playing, so it's reported, not treated as a failure.

Files that claim more tracks than they have are common enough to be worth reporting, not hiding. That's why `DeclaredTrackCount` and `ReadTrackCount` are both here. If they're different, the file isn't what it said it was.
