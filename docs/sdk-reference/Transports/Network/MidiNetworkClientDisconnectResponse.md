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
| `ClientId` | The GUID of the client entry the request referred to |
| `Success` | True if the client was disconnected |
| `ErrorCode` | A `MidiNetworkClientDisconnectErrorCode` when `Success` is false |
| `ErrorMessage` | A human-readable description of the failure |

## Remarks

Check `Success` first. When it is false, `ErrorCode` gives the machine-readable reason and `ErrorMessage` a localized description suitable for display.

Disconnecting a client the service does not have returns `ClientNotFound` rather than reporting success.