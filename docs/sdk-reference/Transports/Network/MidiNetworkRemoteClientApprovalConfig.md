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
| `MidiNetworkRemoteClientApprovalConfig()` | Creates an empty configuration |
| `MidiNetworkRemoteClientApprovalConfig(hostId, remoteClientName, remoteClientProductInstanceId, approve, restrictScopeToThisRequestOnly)` | Creates a configuration with all of these values set |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the client is connecting to |
| `RemoteClientName` | The UMP Endpoint Name of the remote client, from `MidiNetworkPendingRemoteClient` |
| `RemoteClientProductInstanceId` | The Product Instance Id of the remote client |
| `Approve` | True to allow the connection, and false to refuse it |
| `ScopeIsThisRequestOnly` | True to apply the decision only to this request. False to remember it for future connections from the same client |

## Remarks

A remote client is recognized by the `RemoteClientName` and `RemoteClientProductInstanceId` pair, never by its address. A client may use a new source port for every session, and its address can change, so an address is the wrong thing to approve.

When `ScopeIsThisRequestOnly` is false, the service remembers the decision while it runs, and the same client is allowed or refused without asking again. To keep the decision after the service restarts, your app also saves a [MidiNetworkHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostKnownClientsConfig/) for that host, with `MidiServiceTransportPluginConfigManager.SaveUpdate`. The service reads the saved lists when it starts, but never saves them itself.

Taking back a decision like this takes the same two steps in reverse. Save a `MidiNetworkHostKnownClientsConfig` without the client, and call `MidiNetworkTransportManager.ForgetRemoteClientAsync` so the running service forgets it too.