---
layout: sdk_reference_page
title: MidiNetworkRemoteClientForgetConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to drop a remembered allow or deny decision for a remote client
---

Pass to `MidiNetworkTransportManager.ForgetRemoteClientAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkRemoteClientForgetConfig()` | Creates an empty configuration |
| `MidiNetworkRemoteClientForgetConfig(hostId, remoteClientName, remoteClientProductInstanceId)` | Creates a configuration with all of these values set |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry on this PC |
| `RemoteClientName` | The UMP Endpoint Name of the remote client |
| `RemoteClientProductInstanceId` | The Product Instance Id of the remote client |

## Remarks

Forgetting doesn't block anything. A session that's already running keeps running. Only the remembered decision is dropped, so the next invitation from that remote client is judged by the host's remote client policy alone.

This applies to the running service. The saved allow and deny lists are separate, and you save them with `MidiNetworkHostKnownClientsConfig`. An app that forgets a decision should do both, or the decision comes back the next time the service starts.
