---
layout: sdk_reference_page
title: MidiSequenceLyricLine
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: Lyric syllables reassembled into a line you can display
---

A file stores lyrics one syllable at a time, which is unreadable on its own. `MidiSequenceLyricLine` is those syllables put back together into the lines the writer intended. Get them from the `LyricLines` collection on [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/).

## Properties

| Property | Description |
| -------- | ----------- |
| `StartTick` | Where the line begins |
| `EndTick` | Where the next line begins |
| `Text` | The assembled line |

## Remarks

The line breaks come from the file where it marks them, and from the spacing of the singing where it does not, so this is an interpretation rather than something the file states. If you need the raw syllables, they are still in the sequence's `TextEvents` collection as [`MidiSequenceTextEvent`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTextEvent/) entries with `Kind` set to `Lyric`, and you are free to break them yourself.

To find the line in force at a moment, rather than one which starts at it, call `GetLyricLineAtTick` on the sequence.
