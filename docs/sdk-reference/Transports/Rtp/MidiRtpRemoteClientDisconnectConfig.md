---
layout: sdk_reference_page
title: MidiRtpRemoteClientDisconnectConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to end one connection to an RTP-MIDI host on this PC
---

Pass to `MidiRtpTransportManager.DisconnectRemoteClientAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpRemoteClientDisconnectConfig()` | Creates an empty configuration |
| `MidiRtpRemoteClientDisconnectConfig(hostId, connectionId)` | Creates a configuration with both values set |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host on this PC |
| `ConnectionId` | The connection to end, from `MidiRtpConnection.ConnectionId` |

## Remarks

This ends the connection and nothing more. No decision is recorded, so the remote device may connect again. To turn it away in future, deny it with `ApproveOrDenyRemoteClientConnectRequestAsync`.

A connection is named by its id rather than by the remote device's name, because two remote devices can send the same name.
