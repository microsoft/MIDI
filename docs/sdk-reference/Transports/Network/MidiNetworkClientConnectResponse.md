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
| `ClientId` | The GUID of the client entry the request was about |
| `Success` | True if the connection request was accepted |
| `ErrorCode` | A `MidiNetworkClientConnectErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a translated description you can show to people.
