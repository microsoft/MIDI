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

`SingleTrack` doesn't mean one instrument. A format 0 file often has all sixteen channels on its one track. That's why [`MidiSequenceTrack`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrack/) has a `UsedChannelMask`, not a single channel.

`MultiSequence` is rare, and the tracks in a file like that aren't meant to be played at the same time.
