---
layout: sdk_reference_page
title: MidiSequencePlayerState
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: enum
description: What a sequence player is currently doing
---

`MidiSequencePlayerState` is the `State` of a [`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/), and is also carried in [`MidiSequencePlayerPosition`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayerPosition/).

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoSequence` | `0` | No sequence has been loaded yet, so there's nothing to play |
| `Stopped` | `1` | A sequence is loaded, and the position is at the start |
| `Playing` | `2` | Messages are being scheduled and sent |
| `Paused` | `3` | Playback is paused, and the current position is kept |

## Remarks

Handle the player's `StateChanged` event to keep your transport buttons up to date, instead of assuming the state after each call. When playback reaches the end of a sequence, the player leaves `Playing` on its own, and also raises `PlaybackEnded`.
