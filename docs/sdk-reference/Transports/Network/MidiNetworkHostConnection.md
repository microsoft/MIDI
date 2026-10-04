---
layout: sdk_reference_page
title: MidiNetworkHostConnection
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Information about one remote client connected to a host on this PC
---

Returned inside `MidiNetworkConfiguredHost.Connections`.

## Properties

| Property | Description |
| -------- | ----------- |
| `UmpEndpointName` | The UMP Endpoint Name the remote client reported |
| `ProductInstanceId` | The Product Instance Id the remote client reported |
| `RemoteAddress` | The remote IP address right now. For display only. It can change, so don't use it to recognize the client |
| `RemotePort` | The remote source port right now. For display only |
| `IsSessionActive` | True if a session with this remote client is running |
| `IsPendingApproval` | True if the host is waiting for someone to approve or deny this client before letting it in |
| `EndpointDeviceId` | The endpoint device id created for this connection, when it's active |
| `CurrentLatencyTicks` | The latency measured right now, in QPC ticks |
| `RetransmitCount` | How many command packets have been sent again to this client |
| `RetransmitRequestCount` | How many requests to send again have come from this client |
| `TotalCountNetworkPacketsSent` | The total number of network packets sent to this client |
| `TotalCountNetworkPacketsReceived` | The total number of network packets received from this client |
| `CurrentSendSpeedLimit` | How fast the host is sending to this client right now. It's lower than `SendSpeedLimit` while the connection has slowed down by itself. See [MidiNetworkSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSendSpeedLimitEnum/) |
| `SendSpeedLimit` | How fast the host is set up to send to this client. That's the client's own speed when `UsesRemoteClientSettings` is true, and the host's speed otherwise |
| `ReduceSendSpeedAutomatically` | True when the connection sends more slowly while the client keeps asking for data again. Like `SendSpeedLimit`, it comes from the client's own settings or from the host's |
| `UsesRemoteClientSettings` | True when the host has a sending speed of this client's own, in `MidiNetworkConfiguredHost.RemoteClientSettings`, and uses it instead of the host's |

## Remarks

A remote client is recognized by `UmpEndpointName` and `ProductInstanceId`, not by `RemoteAddress` and `RemotePort`, because addresses and ports can change between connections. That's also how the host matches a client to a speed of its own, so a client that announces neither can't have one.
