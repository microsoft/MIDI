---
layout: sdk_reference_page
title: MidiNetworkPendingRemoteClient
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: A remote client waiting for a user decision before it may connect
---

Returned by `MidiNetworkTransportManager.GetPendingRemoteClients()`. Each entry is a remote client which invited one of this PC's hosts, where that host requires approval.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the client is trying to connect to |
| `HostServiceInstanceName` | The mDNS service instance name of that host |
| `HostUmpEndpointName` | The UMP Endpoint Name of that host |
| `UmpEndpointName` | The UMP Endpoint Name the remote client supplied. Show this to the user |
| `ProductInstanceId` | The Product Instance Id the remote client supplied |
| `RemoteAddress` | The address the request arrived from. For display only, not identity |
| `RequestTime` | When the client first asked, in UTC |

## Remarks

`RequestTime` records the first invitation, not the most recent. A waiting client keeps re-inviting on a timer, so this shows how long it has genuinely been waiting.

Approve or deny with `MidiNetworkTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`. Until a decision is made no endpoint or device node is created for the client, so a pending remote costs nothing.