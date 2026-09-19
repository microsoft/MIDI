---
layout: sdk_reference_page
title: MidiSequenceTextEvent
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: A piece of text carried in a sequence, with the tick it belongs to
---

`MidiSequenceTextEvent` is one piece of text carried in a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/), together with where it belongs on the timeline. Get them from the sequence's `TextEvents` collection.

## Properties

| Property | Description |
| -------- | ----------- |
| `Tick` | Where on the timeline this text belongs |
| `TrackIndex` | The track which carried it |
| `Kind` | The [`MidiSequenceTextKind`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTextKindEnum/) of text this is |
| `Text` | The text itself |

## Remarks

Lyrics appear here one syllable at a time, which is how a file stores them and is unreadable on its own. For words you can actually display, use the sequence's `LyricLines` collection of [`MidiSequenceLyricLine`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceLyricLine/) instead. The individual syllables remain here for an application which would rather do its own line breaking.

Chord symbols also arrive as text events, with `Kind` set to `ChordSymbol`. To find the chord in force at a moment rather than the one which happens at it, call `GetChordSymbolAtTick` on the sequence.
