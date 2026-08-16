---
layout: sdk_reference_page
title: MidiNetworkClientConnectResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of a request to connect to a remote Network MIDI 2.0 host
---

Returned by `MidiNetworkTransportManager.ConnectNetworkClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry the request referred to |
| `Success` | True if the connection request was accepted |
| `ErrorCode` | A `MidiNetworkClientConnectErrorCode` when `Success` is false |
| `ErrorMessage` | A human-readable description of the failure |

## Remarks

Check `Success` first. When it is false, `ErrorCode` gives the machine-readable reason and `ErrorMessage` a localized description suitable for display.
