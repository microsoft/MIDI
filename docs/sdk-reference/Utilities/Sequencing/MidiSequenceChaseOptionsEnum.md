---
layout: sdk_reference_page
title: MidiSequenceChaseOptions
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: enum
description: What a sequence player restores when playback starts partway into a sequence
---

`MidiSequenceChaseOptions` is the `ChaseOptions` property of a [`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/). It says what the player sends before the music when playback starts partway into a sequence, so the instrument is left the way it would have been if the sequence had played from the start. Sequencers call this chase.

It's a set of flags, so you can combine them. Added in version 2 of `MidiSequencingUtilityApiContract`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `None` | `0x00000000` | Nothing. Playback starts with the instrument the way it was left |
| `ChannelSettings` | `0x00000001` | The bank, program, controllers, and pitch bend each channel had at that point. Only values the sequence set are sent. This is the default |
| `HeldNotes` | `0x00000002` | Notes that started before that point and end after it, restarted at the velocity they started with |

## Remarks

Use `ChannelSettings | HeldNotes` for organ files that turn a stop on by holding a long note on a control channel, and for drones and pads. Leave `HeldNotes` out for piano and drums. A note restarted partway through starts with a new attack, and on those instruments you hear it.

A note that ends exactly where playback starts isn't restarted, and neither is a note on a muted track.

Some organ setups switch a stop on or off with each note on, instead of holding it on while the note is held. Don't use `HeldNotes` with those. Restarting the note would switch the stop off.
