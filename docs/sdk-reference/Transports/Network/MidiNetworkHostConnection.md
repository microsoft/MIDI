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
| `CurrentSendSpeedLimit` | How fast the host is sending to this client right now. It's lower than the host's `SendSpeedLimit` while the connection has slowed down by itself. See [MidiNetworkSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSendSpeedLimitEnum/) |

## Remarks

A remote client is recognized by `UmpEndpointName` and `ProductInstanceId`, not by `RemoteAddress` and `RemotePort`, because addresses and ports can change between connections.
