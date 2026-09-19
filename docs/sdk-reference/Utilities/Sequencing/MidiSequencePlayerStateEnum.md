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
| `NoSequence` | `0` | No sequence has been loaded yet, so there is nothing to play |
| `Stopped` | `1` | A sequence is loaded and the position is at the start |
| `Playing` | `2` | Messages are being scheduled and sent |
| `Paused` | `3` | Playback is suspended, keeping the current position |

## Remarks

Handle the player's `StateChanged` event to keep your transport buttons in step, rather than assuming the state after each call. Playback reaching the end of a sequence moves the player out of `Playing` on its own, and raises `PlaybackEnded` as well.
