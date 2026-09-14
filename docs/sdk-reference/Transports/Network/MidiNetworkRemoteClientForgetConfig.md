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
| `MidiNetworkRemoteClientForgetConfig()` | Create an empty config |
| `MidiNetworkRemoteClientForgetConfig(hostId, remoteClientName, remoteClientProductInstanceId)` | Create a fully populated config |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The host entry GUID on this PC |
| `RemoteClientName` | UMP Endpoint Name of the remote client |
| `RemoteClientProductInstanceId` | Product Instance Id of the remote client |

## Remarks

Forgetting is not blocking. A session which is already established is left running; only the
remembered decision is dropped, so the next invitation from that remote client is judged on the
host's remote client policy alone.

This applies to the running service. The saved allow and deny lists are separate, and are rewritten
with `MidiNetworkHostKnownClientsConfig`. An application which forgets a decision should do both,
or the decision returns the next time the service starts.
