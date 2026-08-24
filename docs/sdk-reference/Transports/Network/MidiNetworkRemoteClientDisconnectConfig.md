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
| `MidiNetworkRemoteClientDisconnectConfig()` | Create an empty config |
| `MidiNetworkRemoteClientDisconnectConfig(hostId, remoteClientName, remoteClientProductInstanceId)` | Create a fully populated config |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The host entry GUID on this PC |
| `RemoteClientName` | UMP Endpoint Name of the remote client |
| `RemoteClientProductInstanceId` | Product Instance Id of the remote client |

## Remarks

This request ends the current session only. It does not persist an allow/deny decision for future connection attempts.
