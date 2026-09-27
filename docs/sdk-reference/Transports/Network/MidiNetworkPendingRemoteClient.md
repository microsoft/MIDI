---
layout: sdk_reference_page
title: MidiNetworkPendingRemoteClient
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: A remote client waiting for a user decision before it may connect
---

Returned by `MidiNetworkTransportManager.GetPendingRemoteClients()`. Each entry is a remote client that sent an invitation to one of this PC's hosts, where that host requires approval.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the client is trying to connect to |
| `HostServiceInstanceName` | The mDNS service instance name of that host |
| `HostUmpEndpointName` | The UMP Endpoint Name of that host |
| `UmpEndpointName` | The UMP Endpoint Name the remote client sent. Show this to people |
| `ProductInstanceId` | The Product Instance Id the remote client sent |
| `RemoteAddress` | The address the request came from. For display only. Don't use it to recognize the client |
| `RequestTime` | When the client first asked, in UTC |

## Remarks

`RequestTime` is when the first invitation came, not the most recent one. A waiting client keeps sending invitations on a timer, so this shows how long it has been waiting.

Approve or deny it with `MidiNetworkTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`. Until someone decides, no endpoint or device node is created for the client, so a waiting client costs nothing.