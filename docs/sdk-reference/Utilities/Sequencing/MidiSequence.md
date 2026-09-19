---
layout: sdk_reference_page
title: MidiSequence
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: Music on a timeline, ready to be played or drawn
---

`MidiSequence` is music on a timeline, ready to be played or drawn. It is deliberately not tied to any file format: a Standard MIDI File produces one, a MIDI Clip File produces one, and so does an application building a sequence itself.

A sequence is a handle over its own storage rather than a collection of message objects. Files in the wild reach millions of events, so the dense parts of a sequence are never projected one item at a time. The sparse parts, which a display genuinely needs in full, are ordinary collections.

## Timing Properties

| Property | Description |
| -------- | ----------- |
| `Format` | The [`MidiSequenceFormat`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceFormatEnum/) of the sequence |
| `TicksPerQuarterNote` | Ticks per quarter note, unless the sequence is timed in SMPTE frames |
| `UsesSmpteTiming` | True when the tick is an absolute division of a second rather than a musical division, in which case tempo does not apply |
| `DurationMicroseconds` | Total length of the sequence |
| `LastTick` | The tick of the final event |

## Content Properties

| Property | Description |
| -------- | ----------- |
| `EventCount` | Total number of events |
| `NoteCount` | Total number of paired notes |
| `Title` | The title, where the sequence carried one |
| `Copyright` | The copyright text, where the sequence carried one |
| `IsKaraoke` | True when the file announced itself as a karaoke file, which changes where its words are stored. Worth surfacing because it tells your application there are words to show |
| `LowestNoteNumber` | Lowest note number actually played |
| `HighestNoteNumber` | Highest note number actually played |
| `LongestNoteTicks` | How far back a search has to reach to find every note which could still be sounding at a given tick |
| `UsedChannelMask` | Bit zero is channel one |

`LowestNoteNumber` and `HighestNoteNumber` let a display fill its height with the music instead of all 128 notes.

Notes are ordered by where they start, so a note which began before a window can still be sounding inside it. `LongestNoteTicks` is how far back you have to look to catch those.

## Collections

These are sparse enough to hand over whole. Each is built once while reading and cached.

| Property | Description |
| -------- | ----------- |
| `Tracks` | The [`MidiSequenceTrack`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrack/) collection |
| `TempoMap` | Every [`MidiSequenceTempoChange`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTempoChange/) in the sequence |
| `TimeSignatureMap` | Every [`MidiSequenceTimeSignature`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimeSignature/) in the sequence |
| `TextEvents` | Every [`MidiSequenceTextEvent`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTextEvent/), including individual lyric syllables |
| `LyricLines` | Syllables reassembled into [`MidiSequenceLyricLine`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceLyricLine/) lines |

## Position Methods

Both conversions are backed by a map built while reading, so neither walks the sequence.

| Method | Description |
| ------ | ----------- |
| `ConvertTickToMicroseconds(tick)` | Where a tick falls in time |
| `ConvertMicrosecondsToTick(microseconds)` | Where a moment in time falls in ticks |
| `GetBeatsPerMinuteAtTick(tick)` | The tempo in force at that tick |
| `GetBarPositionAtTick(tick)` | The [`MidiSequenceBarPosition`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceBarPosition/) at that tick |

## Lookup Methods

These return what is *in force* at a moment, rather than what happens at it. Both return null before the first one in the sequence.

| Method | Description |
| ------ | ----------- |
| `GetChordSymbolAtTick(tick)` | The chord symbol in force, as a text event |
| `GetLyricLineAtTick(tick)` | The lyric line in force |

## Note Methods

Notes are the dense part of a sequence. Ask for a window and fill an array you own, so drawing a frame costs one call rather than one per note.

| Method | Description |
| ------ | ----------- |
| `GetNoteCountInTickRange(startTick, endTick)` | How many notes fall in the window, so you can size your array |
| `FillNotesInTickRange(startTick, endTick, startIndex, notes)` | Fills your array with [`MidiSequenceNote`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceNote/) values and returns the number written, capped by the length of the array you supplied |
| `FillSoundingNoteCountsAtTick(tick, countsPerTrack)` | Fills your array with how many notes each track is holding at that moment, one entry per track, for a level display |

## Remarks

Subtract `LongestNoteTicks` from your window start before calling `FillNotesInTickRange` if you need notes which began earlier and are still sounding. The sequence orders notes by start tick, so it cannot find them for you otherwise.
