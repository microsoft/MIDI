---
layout: sdk_reference_page
title: MidiNetworkClientDisconnectConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Disconnects a Network MIDI 2.0 client, or forgets a saved one
---

Pass to `MidiNetworkTransportManager.DisconnectNetworkClientAsync` to disconnect a client now. Pass to `MidiServiceTransportPluginConfigManager.SaveUpdate` to remove the saved client, so the service doesn't connect it the next time it starts.

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

Disconnecting and forgetting are separate steps, like connecting and saving. To do both, call `DisconnectNetworkClientAsync` and then `SaveUpdate` with the same object.