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
| `TrackIndex` | Index of this track within the sequence |
| `Name` | The track name, where the file carried one |
| `InstrumentName` | The instrument name, where the file carried one |
| `SuggestedDeviceName` | The port the track was written for, when the file names one |
| `UsedChannelMask` | Bit zero is channel one. A track is not obliged to use only one |
| `NoteCount` | Number of paired notes on this track |
| `EventCount` | Number of events on this track |
| `LastTick` | Tick of the final event on this track |

## Remarks

A file meant for several instruments at once carries `SuggestedDeviceName` on each track, so it is the starting point for deciding where a track should be sent. Use it to build a default [`MidiSequenceTrackRouting`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrackRouting/), but let the person using your application override it.

Do not assume one channel per track. `UsedChannelMask` exists because plenty of real files put several channels on a single track, and a format 0 file puts all sixteen on one.
