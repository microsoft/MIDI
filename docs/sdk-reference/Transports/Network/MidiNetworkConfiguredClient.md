---
layout: sdk_reference_page
title: MidiNetworkConfiguredClient
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Information about a Network MIDI 2.0 client connection configured in the service
---

Returned by `MidiNetworkTransportManager.GetConfiguredClients()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID that identifies this client entry |
| `IsSessionActive` | True if a MIDI session is set up right now |
| `EntryState` | Where this entry is in its life. See `MidiNetworkClientEntryState` |
| `IsDirectConnection` | True if this client was set up with an address and port, instead of being discovered |
| `ConfiguredDirectAddress` | The remote address it was set up with, for a direct connection |
| `ConfiguredDirectPort` | The remote port it was set up with, for a direct connection |
| `MatchDeviceId` | The Windows device id of the discovered host this client connects to. Empty for direct connections |
| `ConnectedRemoteAddress` | The remote address in use now |
| `ConnectedRemotePort` | The remote port in use now |
| `ConnectedLocalAddress` | The local address in use now |
| `ConnectedLocalPort` | The local port in use now |
| `EndpointDeviceId` | The device id of the MIDI endpoint created for this connection |
| `RetransmitCount` | How many times messages have been sent again to this remote device. For troubleshooting |
| `RetransmitRequestCount` | How many requests to send again have come from this remote device. For troubleshooting |
| `CurrentLatencyTicks` | The measured latency, in ticks. For troubleshooting. Reading this resets the running average |
| `TotalCountNetworkPacketsSent` | The total number of network packets sent on this connection |
| `TotalCountNetworkPacketsReceived` | The total number of network packets received on this connection |
| `LastErrorCode` | Why the last invitation didn't open a session, such as `NoReplyToInvitation` or `AuthenticationRequired`. `NoErrorInformationAvailable` once a session opens. See `MidiNetworkClientConnectErrorCode` |
| `SendSpeedLimit` | How fast this PC is set up to send to the remote host. See [MidiNetworkSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSendSpeedLimitEnum/) |
| `ReduceSendSpeedAutomatically` | True when the connection sends more slowly while the remote host keeps asking for data again |
| `SkipInBetweenValuesToCatchUp` | True when the connection skips some in-between values to catch up when messages pile up behind `SendSpeedLimit`. Added in version 2 of `MidiTransportsNetworkApiContract` |
| `CurrentSendSpeedLimit` | How fast the connection is sending right now. It's lower than `SendSpeedLimit` while the connection has slowed down by itself, which is worth showing people |
| `WaitingForHostId` | A device has one session with this PC at a time. When the remote host is already connected to one of this PC's hosts as a client, this is that host's `HostId`, and the entry waits: it stops trying, and connects once that session ends. Equals `GuidHelper.Empty()` when not set. At most one of `WaitingForHostId` and `WaitingForClientId` is set at a time. Added in version 2 of `MidiTransportsNetworkApiContract` |
| `WaitingForClientId` | When this PC is already connected to the remote host through another client entry, this is that entry's `ClientId`, and this entry waits for that session to end the same way. Equals `GuidHelper.Empty()` when not set. At most one of `WaitingForHostId` and `WaitingForClientId` is set at a time. Added in version 2 of `MidiTransportsNetworkApiContract` |

## Remarks

Every client that's set up is reported, whether or not it's connected, so an entry that has never reached its remote host still shows up. Use `EntryState` to tell the cases apart, and `LastErrorCode` to tell the person why. When there's no session running, use the `Configured*` properties, not the `Connected*` ones.

`LastErrorCode` stays set while the service keeps trying. A `Pending` entry with `NoReplyToInvitation` is a host that isn't answering and is being tried again. A `Failed` entry with `AuthenticationRequired` is a host that wants a password, which Windows doesn't support yet. A `Failed` entry with `InvitationEndedByHost` is a host that ended the session on purpose: its user disconnected, or it's powering down. Windows doesn't connect it again until it's back on the network, or until the entry is connected again. Connecting the entry again clears it.

To tell whether an entry is waiting for another session with the same device, check whether either `WaitingForHostId` or `WaitingForClientId` isn't `GuidHelper.Empty()`. A waiting entry is `Pending`, its `LastErrorCode` is `NoErrorInformationAvailable`, and it isn't trying to connect.

`CurrentLatencyTicks` is an average that resets each time it's read. If you're graphing it, read it at a steady interval.