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
| `Copyright` | `2` | Copyright notice |
| `TrackName` | `3` | Name of the track, or of the whole sequence when it is on the first track |
| `InstrumentName` | `4` | Instrument the track was written for |
| `Lyric` | `5` | A single lyric syllable |
| `Marker` | `6` | A named point in the timeline |
| `CuePoint` | `7` | A cue for something happening alongside the music |
| `ProgramName` | `8` | Name of the patch the track expects |
| `DeviceName` | `9` | Name of the port the track was written for |
| `ChordSymbol` | `20` | A chord name |

## Remarks

The values from `1` through `9` are the meta event types a file uses for text, so they match the file format exactly.

`ChordSymbol` is deliberately outside that range. It does not come from a meta event at all, but from a manufacturer System Exclusive message that lead sheet and karaoke files use to carry chord names, because Standard MIDI Files never defined an event for them. Treat the numbering gap as a signal that this one is different in origin.

Lyric syllables arrive individually under `Lyric`. Use the sequence's `LyricLines` collection for words you can actually display.
