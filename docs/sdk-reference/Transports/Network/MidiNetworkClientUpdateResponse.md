---
layout: sdk_reference_page
title: MidiNetworkClientUpdateResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of a request to change settings on a Network MIDI 2.0 client connection
---

Returned by `MidiNetworkTransportManager.UpdateNetworkClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry the request referred to |
| `Success` | True if the settings were applied |
| `ErrorCode` | A `MidiNetworkClientUpdateErrorCode` when `Success` is false |
| `ErrorMessage` | A human-readable description of the failure |

## Remarks

Check `Success` first. When it is false, `ErrorCode` gives the machine-readable reason and `ErrorMessage` a localized description suitable for display.

Updating a client the service does not have returns `ClientNotFound` rather than reporting success.

`Success` means the service accepted and applied the settings, not that every one of them changed something observable. A setting which only takes effect on the next connection, or which is outranked by the remote's function blocks, still reports success. See [MidiNetworkClientUpdateConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientUpdateConfig/) for which is which.
