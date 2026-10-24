---
layout: sdk_reference_page
title: MidiNetworkRefusedRemoteClient
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: A device a host turned away because it already had a session with this PC
---

Added in version 2 of `MidiTransportsNetworkApiContract`.

Reported in the `RecentlyRefusedSecondConnections` and `RecentlyRefusedRemoteHosts` lists of [MidiNetworkConfiguredHost]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkConfiguredHost/). You can't create one yourself.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host that turned the device away. Always set: never `GuidHelper.Empty()` |
| `UmpEndpointName` | The UMP Endpoint Name the device sent. Show this to people |
| `ProductInstanceId` | The Product Instance Id the device sent |
| `RemoteAddress` | The address the invitation came from. For display only. Don't use it to recognize the device |
| `RefusedTime` | When the host last turned the device away, in UTC |

## Remarks

A device can have only one Network MIDI 2.0 session with this PC at a time. When it tries to open a second one, the host says no. Without this list, the person at the PC has no way to tell why the device didn't connect.

A device that keeps trying stays one entry, and `RefusedTime` moves to its latest try. Entries go away a few minutes after the last try.
