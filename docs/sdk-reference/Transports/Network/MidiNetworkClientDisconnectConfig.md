---
layout: sdk_reference_page
title: MidiNetworkClientDisconnectConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to disconnect a Network MIDI 2.0 client
---

Pass to `MidiNetworkTransportManager.DisconnectNetworkClientAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkClientDisconnectConfig()` | Creates an empty configuration |
| `MidiNetworkClientDisconnectConfig(clientId)` | Creates a configuration for this client |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry to disconnect |

## Remarks

A client disconnected this way stays disconnected. The service doesn't treat a disconnect someone asked for as a lost connection, so it doesn't reconnect it automatically.