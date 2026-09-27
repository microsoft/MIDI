---
layout: sdk_reference_page
title: MidiSequenceTextKind
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: enum
description: What kind of text a sequence text event carries
---

`MidiSequenceTextKind` says what a [`MidiSequenceTextEvent`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTextEvent/) is carrying.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Text` | `1` | General text |
| `Copyright` | `2` | A copyright notice |
| `TrackName` | `3` | The name of the track, or of the whole sequence when it's on the first track |
| `InstrumentName` | `4` | The instrument the track was written for |
| `Lyric` | `5` | One lyric syllable |
| `Marker` | `6` | A named point in the timeline |
| `CuePoint` | `7` | A cue for something happening along with the music |
| `ProgramName` | `8` | The name of the sound the track expects |
| `DeviceName` | `9` | The name of the port the track was written for |
| `ChordSymbol` | `20` | A chord name |

## Remarks

The values from `1` through `9` are the meta event types a file uses for text, so they match the file format exactly.

`ChordSymbol` is outside that range on purpose. It doesn't come from a meta event at all. It comes from a manufacturer's System Exclusive message that lead sheet and karaoke files use for chord names, because Standard MIDI Files never defined an event for them. The gap in the numbers is a sign that this one comes from somewhere different.

Lyric syllables come one at a time as `Lyric`. Use the sequence's `LyricLines` collection for words you can show on screen.
