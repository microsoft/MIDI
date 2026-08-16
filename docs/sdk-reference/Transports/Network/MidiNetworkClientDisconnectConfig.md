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
| `MidiNetworkClientDisconnectConfig()` | Create an empty config |
| `MidiNetworkClientDisconnectConfig(clientId)` | Create a config for the specified client |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry to disconnect |

## Remarks

A client disconnected this way stays disconnected. The service does not treat a user-requested disconnect as a connection loss, so it is not reconnected automatically.