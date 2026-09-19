---
layout: sdk_reference_page
title: MidiSequenceTrackRouting
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: Where one track's messages are sent during playback
---

`MidiSequenceTrackRouting` decides where one track's messages are sent. A file written for several instruments at once names the port it wanted for each track, so a sensible default can be worked out, but the choice belongs to your application.

Use it with `GetTrackRouting` and `SetTrackRouting` on [`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/).

## Properties

| Property | Description |
| -------- | ----------- |
| `Connection` | The [`MidiEndpointConnection`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection/) this track is sent to. Null to use whichever connection the player was created with |
| `Group` | The [`MidiGroup`]({{ site.baseurl }}/sdk-reference/MidiGroup/) to send on |
| `ChannelOverride` | Set to move a track onto a different [`MidiChannel`]({{ site.baseurl }}/sdk-reference/MidiChannel/) than the file wrote it for. Null leaves the channel in the file alone |
| `IsMuted` | Whether this track is silenced |

## Remarks

Tracks may be sent to different endpoints at the same time and still stay together, because every message is scheduled against the same machine-wide clock. That is what makes it reasonable to drive a rack of hardware from one file.

[`MidiSequenceTrack.SuggestedDeviceName`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrack/) is the starting point for a default. Match it against the endpoints on the PC to pre-select a connection, then let the person using your application change it.
