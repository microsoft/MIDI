---
layout: sdk_reference_page
title: MidiRtpClientDisconnectConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to disconnect and remove an RTP-MIDI client
---

Pass it to `MidiRtpTransportManager.DisconnectRtpClientAsync` to end a client's connection and remove the client from the running service, and to `MidiServiceTransportPluginConfigManager.SaveUpdate` to take it out of the configuration.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpClientDisconnectConfig()` | Creates an empty configuration |
| `MidiRtpClientDisconnectConfig(clientId)` | Creates a configuration for this client |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry to remove |

## Remarks

This removes the entry, so the service doesn't connect it again, and its endpoint goes away.

`DisconnectRtpClientAsync` only changes the running service, and `SaveUpdate` only changes the configuration. To remove a client for good, do both.
