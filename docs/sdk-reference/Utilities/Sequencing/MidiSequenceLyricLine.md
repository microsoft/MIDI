---
layout: sdk_reference_page
title: MidiSequenceLyricLine
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: Lyric syllables reassembled into a line you can display
---

A file stores lyrics one syllable at a time, which is hard to read by itself. `MidiSequenceLyricLine` puts those syllables back together into the lines the writer meant. Get them from the `LyricLines` collection on [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/).

## Properties

| Property | Description |
| -------- | ----------- |
| `StartTick` | Where the line begins |
| `EndTick` | Where the next line begins |
| `Text` | The whole line |

## Remarks

The line breaks come from the file where it marks them, and from the timing of the singing where it doesn't. So this is a best guess, not something the file says. If you need the raw syllables, they're still in the sequence's `TextEvents` collection, as [`MidiSequenceTextEvent`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTextEvent/) entries with `Kind` set to `Lyric`, and you can break them into lines yourself.

To find the line in effect at a moment, not one that starts exactly then, call `GetLyricLineAtTick` on the sequence.
