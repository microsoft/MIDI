---
layout: sdk_reference_page
title: MidiSequenceTextEvent
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: A piece of text carried in a sequence, with the tick it belongs to
---

`MidiSequenceTextEvent` is one piece of text carried in a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/), along with where it belongs on the timeline. Get them from the sequence's `TextEvents` collection.

## Properties

| Property | Description |
| -------- | ----------- |
| `Tick` | Where on the timeline this text belongs |
| `TrackIndex` | The track it's on |
| `Kind` | What kind of text this is, as a [`MidiSequenceTextKind`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTextKindEnum/) |
| `Text` | The text itself |

## Remarks

Lyrics show up here one syllable at a time, which is how a file stores them, and that's hard to read by itself. For words you can show on screen, use the sequence's `LyricLines` collection of [`MidiSequenceLyricLine`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceLyricLine/) instead. The single syllables stay here for an app that would rather break lines itself.

Chord symbols also come as text events, with `Kind` set to `ChordSymbol`. To find the chord in effect at a moment, not one that happens exactly then, call `GetChordSymbolAtTick` on the sequence.
