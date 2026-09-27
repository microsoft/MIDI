---
layout: sdk_reference_page
title: MidiSequence
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: Music on a timeline, ready to be played or drawn
---

`MidiSequence` is music on a timeline, ready to be played or drawn. It isn't tied to any file format, on purpose. A Standard MIDI File gives you one, and so does an app that builds a sequence itself.

A sequence isn't a collection of message objects. It's a way to reach the data it stores. Real files can have millions of events, so the busiest parts of a sequence are never handed to your app one item at a time. The smaller parts, which a display needs all of, are normal collections.

## Timing Properties

| Property | Description |
| -------- | ----------- |
| `Format` | The [`MidiSequenceFormat`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceFormatEnum/) of the sequence |
| `TicksPerQuarterNote` | Ticks per quarter note, unless the sequence is timed in SMPTE frames |
| `UsesSmpteTiming` | True when a tick is a fixed part of a second instead of a musical length. Then tempo doesn't apply |
| `TimingMode` | [`MidiSequenceTimingMode`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimingModeEnum/). Whether the ticks above are musical, or are just microseconds. A sequence read from a file is always `Musical`. Only one built with [`MidiSequenceBuilder`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceBuilder/) can be `Absolute` |
| `DurationMicroseconds` | The total length of the sequence |
| `LastTick` | The tick of the last event |

## Content Properties

| Property | Description |
| -------- | ----------- |
| `EventCount` | The total number of events |
| `NoteCount` | The total number of notes, each with its start and end paired up |
| `Title` | The title, if the sequence has one |
| `Copyright` | The copyright text, if the sequence has it |
| `IsKaraoke` | True when the file says it's a karaoke file, which changes where its words are stored. It's worth showing, because it tells your app there are words to display |
| `LowestNoteNumber` | The lowest note number that's really played |
| `HighestNoteNumber` | The highest note number that's really played |
| `LongestNoteTicks` | The length of the longest note. It tells you how far back to look to find every note that could still be sounding at a given tick |
| `UsedChannelMask` | Which channels are used, one bit per channel. Bit zero is channel one |

`LowestNoteNumber` and `HighestNoteNumber` let a display fill its height with the music instead of all 128 notes.

Notes are sorted by where they start, so a note that began before a window can still be sounding inside it. `LongestNoteTicks` is how far back you have to look to catch those.

## Collections

These are small enough to hand over whole. Each is built once, while reading, and kept.

| Property | Description |
| -------- | ----------- |
| `Tracks` | The [`MidiSequenceTrack`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrack/) collection |
| `TempoMap` | Every [`MidiSequenceTempoChange`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTempoChange/) in the sequence |
| `TimeSignatureMap` | Every [`MidiSequenceTimeSignature`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimeSignature/) in the sequence |
| `TextEvents` | Every [`MidiSequenceTextEvent`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTextEvent/), including single lyric syllables |
| `LyricLines` | Syllables put back together into [`MidiSequenceLyricLine`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceLyricLine/) lines |

## Position Methods

Both conversions use a map built while reading, so neither one has to go through the whole sequence.

| Method | Description |
| ------ | ----------- |
| `ConvertTickToMicroseconds(tick)` | Where a tick falls in time |
| `ConvertMicrosecondsToTick(microseconds)` | Where a moment in time falls in ticks |
| `GetBeatsPerMinuteAtTick(tick)` | The tempo at that tick |
| `GetBarPositionAtTick(tick)` | The [`MidiSequenceBarPosition`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceBarPosition/) at that tick |

## Lookup Methods

These return what's *in effect* at a moment, not what happens exactly then. Both return null before the first one in the sequence.

| Method | Description |
| ------ | ----------- |
| `GetChordSymbolAtTick(tick)` | The chord symbol in effect, as a text event |
| `GetLyricLineAtTick(tick)` | The lyric line in effect |

## Note Methods

Notes are the large part of a sequence. Ask for a window of time and fill an array you own, so drawing a frame takes one call, not one per note.

| Method | Description |
| ------ | ----------- |
| `GetNoteCountInTickRange(startTick, endTick)` | How many notes fall in the window, so you know how big to make your array |
| `FillNotesInTickRange(startTick, endTick, startIndex, notes)` | Fills your array with [`MidiSequenceNote`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceNote/) values, and returns how many it wrote. It never writes more than your array holds |
| `FillSoundingNoteCountsAtTick(tick, countsPerTrack)` | Fills your array with how many notes each track is holding at that moment, one entry per track, for a level meter |

## Remarks

If you need notes that began earlier and are still sounding, subtract `LongestNoteTicks` from the start of your window before you call `FillNotesInTickRange`. The sequence sorts notes by their start tick, so it can't find them for you any other way.
