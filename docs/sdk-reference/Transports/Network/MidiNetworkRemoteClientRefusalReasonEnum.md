---
layout: sdk_reference_page
title: MidiNetworkRemoteClientRefusalReason
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Why a host turned a remote client away
---

Added in version 2 of `MidiTransportsNetworkApiContract`.

Reported by `MidiNetworkRefusedRemoteClient.Reason`, in the `RecentlyRefusedRemoteClients` list of [MidiNetworkConfiguredHost]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkConfiguredHost/). A device can have only one Network MIDI 2.0 session with this PC at a time, so the host turns away a second one.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Unknown` | `0` | A reason this version of the API doesn't know about |
| `AlreadyConnectedToThisHost` | `1` | The device already has a session with this host |
| `ThisPcAlreadyConnectedToRemote` | `2` | This PC is already connected to the device as a client, through a client entry. A device connects to this PC in only one direction at a time |