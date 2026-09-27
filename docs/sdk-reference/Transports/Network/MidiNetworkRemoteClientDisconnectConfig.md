---
layout: sdk_reference_page
title: MidiNetworkRemoteClientDisconnectConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to end one remote client's active session with a host on this PC
---

Pass to `MidiNetworkTransportManager.DisconnectRemoteClientAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkRemoteClientDisconnectConfig()` | Creates an empty configuration |
| `MidiNetworkRemoteClientDisconnectConfig(hostId, remoteClientName, remoteClientProductInstanceId)` | Creates a configuration with all of these values set |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry on this PC |
| `RemoteClientName` | The UMP Endpoint Name of the remote client |
| `RemoteClientProductInstanceId` | The Product Instance Id of the remote client |

## Remarks

This only ends the current session. It doesn't save an allow or deny decision for future connections.
