---
layout: sdk_reference_page
title: MidiSequenceTrackRouting
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: Where one track's messages are sent during playback
---

`MidiSequenceTrackRouting` decides where one track's messages go. A file written for several instruments at once names the port it wanted for each track, so you can work out a good default. But the choice is up to your app.

Use it with `GetTrackRouting` and `SetTrackRouting` on [`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/).

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiSequenceTrackRouting()` | Creates an empty routing. Then set the properties you want |

## Properties

| Property | Description |
| -------- | ----------- |
| `Connection` | The [`MidiEndpointConnection`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection/) this track goes to. Null to use the connection the player was created with |
| `Group` | The [`MidiGroup`]({{ site.baseurl }}/sdk-reference/MidiGroup/) to send on |
| `ChannelOverride` | Set this to move a track to a different [`MidiChannel`]({{ site.baseurl }}/sdk-reference/MidiChannel/) than the file wrote it for. Null leaves the file's channel alone |
| `IsMuted` | Whether this track is silenced |

## Remarks

Tracks can go to different endpoints at the same time and still stay in time with each other, because every message is scheduled against the same clock for the whole PC. That's what makes it practical to play a rack of hardware from one file.

Start with [`MidiSequenceTrack.SuggestedDeviceName`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrack/) for a default. Match it against the endpoints on the PC to pick a connection ahead of time, and then let the person using your app change it.
