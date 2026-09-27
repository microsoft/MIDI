---
layout: sdk_reference_page
title: MidiSequenceTrack
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: One track of a sequence, and what the file said about it
---

`MidiSequenceTrack` describes one track of a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/). Get them from the sequence's `Tracks` collection.

## Properties

| Property | Description |
| -------- | ----------- |
| `TrackIndex` | The track's index in the sequence |
| `Name` | The track name, if the file has one |
| `InstrumentName` | The instrument name, if the file has one |
| `SuggestedDeviceName` | The port the track was written for, if the file names one |
| `UsedChannelMask` | Which channels the track uses, one bit per channel. Bit zero is channel one. A track doesn't have to use only one channel |
| `NoteCount` | How many notes are on this track, each with its start and end paired up |
| `EventCount` | How many events are on this track |
| `LastTick` | The tick of the last event on this track |

## Remarks

A file meant for several instruments at once has a `SuggestedDeviceName` on each track, so start there when you decide where a track should go. Use it to build a default [`MidiSequenceTrackRouting`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrackRouting/), but let the person using your app change it.

Don't assume one channel per track. `UsedChannelMask` exists because plenty of real files put several channels on a single track, and a format 0 file puts all sixteen on one.
