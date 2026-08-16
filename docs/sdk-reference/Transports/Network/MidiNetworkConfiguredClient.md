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
| `ClientId` | The GUID which identifies this client entry |
| `HostId` | The GUID of the host this client is associated with |
| `IsSessionActive` | True if a MIDI session is currently established |
| `EntryState` | Where this entry is in its life. See `MidiNetworkClientEntryState` |
| `IsDirectConnection` | True if this client was configured with an address and port rather than discovered |
| `ConfiguredDirectAddress` | The configured remote address, for a direct connection |
| `ConfiguredDirectPort` | The configured remote port, for a direct connection |
| `ConnectedRemoteAddress` | The remote address currently in use |
| `ConnectedRemotePort` | The remote port currently in use |
| `ConnectedLocalAddress` | The local address currently in use |
| `ConnectedLocalPort` | The local port currently in use |
| `EndpointDeviceId` | The device id of the MIDI endpoint created for this connection |
| `RetransmitCount` | Number of times messages have been retransmitted to this remote. Diagnostic |
| `RetransmitRequestCount` | Number of retransmit requests received from this remote. Diagnostic |
| `CurrentLatencyTicks` | Measured latency in ticks. Diagnostic. Reading this resets the running average |
| `TotalCountNetworkPacketsSent` | Total datagrams sent on this connection |
| `TotalCountNetworkPacketsReceived` | Total datagrams received on this connection |

## Remarks

Every configured client is reported, whether or not it is connected, so an entry which has never reached its remote host still appears. Use `EntryState` to tell the cases apart, and the `Configured*` properties rather than the `Connected*` ones when there is no live session.

`CurrentLatencyTicks` is an average which resets each time it is read, so poll it at a steady interval if you are charting it.