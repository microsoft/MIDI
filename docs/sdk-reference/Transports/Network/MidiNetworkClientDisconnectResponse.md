---
layout: sdk_reference_page
title: MidiNetworkClientDisconnectResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of a request to disconnect a Network MIDI 2.0 client
---

Returned by `MidiNetworkTransportManager.DisconnectNetworkClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry the request was about |
| `Success` | True if the client was disconnected |
| `ErrorCode` | A `MidiNetworkClientDisconnectErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a translated description you can show to people.

Disconnecting a client the service doesn't have returns `ClientNotFound`, not success.