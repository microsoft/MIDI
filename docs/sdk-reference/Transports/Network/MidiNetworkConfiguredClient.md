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

## Remarks

Every client that's set up is reported, whether or not it's connected, so an entry that has never reached its remote host still shows up. Use `EntryState` to tell the cases apart, and `LastErrorCode` to tell the person why. When there's no session running, use the `Configured*` properties, not the `Connected*` ones.

`LastErrorCode` stays set while the service keeps trying. A `Pending` entry with `NoReplyToInvitation` is a host that isn't answering and is being tried again. A `Failed` entry with `AuthenticationRequired` is a host that wants a password, which Windows doesn't support yet. Connecting the entry again clears it.

`CurrentLatencyTicks` is an average that resets each time it's read. If you're graphing it, read it at a steady interval.