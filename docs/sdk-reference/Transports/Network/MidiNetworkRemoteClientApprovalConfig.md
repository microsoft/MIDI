---
layout: sdk_reference_page
title: MidiNetworkRemoteClientApprovalConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to approve or deny a waiting remote client
---

Pass to `MidiNetworkTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkRemoteClientApprovalConfig()` | Create an empty config |
| `MidiNetworkRemoteClientApprovalConfig(hostId, remoteClientName, remoteClientProductInstanceId, approve, restrictScopeToThisRequestOnly)` | Create a fully populated config |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the client is connecting to |
| `RemoteClientName` | The UMP Endpoint Name of the remote client, from `MidiNetworkPendingRemoteClient` |
| `RemoteClientProductInstanceId` | The Product Instance Id of the remote client |
| `Approve` | True to allow the connection, false to refuse it |
| `ScopeIsThisRequestOnly` | True to apply the decision only to the request in hand. False to remember it for future connections from the same client |

## Remarks

A remote client is identified by the `RemoteClientName` and `RemoteClientProductInstanceId` pair, never by address. A client may use a new source port for every session and its address can change, so an address is the wrong thing to approve.

When `ScopeIsThisRequestOnly` is false the service remembers the decision for as long as it runs, and the same client is allowed or refused without asking again. To make it survive a service restart, the calling application also writes the identity into the `allowedClients` or `deniedClients` list for that host in the configuration file. The service reads those lists at startup but never writes them.