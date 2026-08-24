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
| `UmpEndpointName` | UMP Endpoint Name reported by the remote client |
| `ProductInstanceId` | Product Instance Id reported by the remote client |
| `RemoteAddress` | Current remote IP address. For display only; not stable for identity matching |
| `RemotePort` | Current remote source port. For display only |
| `IsSessionActive` | True if a transport session is active with this remote client |
| `IsPendingApproval` | True if the host is waiting for user approval/denial before admitting this client |
| `EndpointDeviceId` | The endpoint device id created for this connection, when active |
| `CurrentLatencyTicks` | Current measured latency in QPC ticks |
| `RetransmitCount` | Count of command packets resent to this client |
| `RetransmitRequestCount` | Count of retransmit requests received from this client |
| `TotalCountNetworkPacketsSent` | Total network packets sent to this client |
| `TotalCountNetworkPacketsReceived` | Total network packets received from this client |

## Remarks

A remote client is identified by `UmpEndpointName` and `ProductInstanceId`, not by `RemoteAddress` and `RemotePort`, because addresses and ports can change between reconnects.
