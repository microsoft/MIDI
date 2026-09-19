---
layout: sdk_reference_page
title: MidiSequenceFormat
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: enum
description: How the tracks in a sequence relate to each other
---

`MidiSequenceFormat` describes how the tracks in a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) relate to each other. It matches the format field of a Standard MIDI File.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `SingleTrack` | `0` | Everything is on one track |
| `MultiTrack` | `1` | Tracks play together |
| `MultiSequence` | `2` | Tracks are independent sequences |

## Remarks

`SingleTrack` does not mean one instrument. A format 0 file routinely carries all sixteen channels on its single track, which is why [`MidiSequenceTrack`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrack/) has a `UsedChannelMask` rather than a single channel.

`MultiSequence` is rare, and the tracks in such a file are not meant to be played at the same time.
