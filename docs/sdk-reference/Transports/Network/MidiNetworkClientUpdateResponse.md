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
| `ClientId` | The GUID of the client entry the request was about |
| `Success` | True if the settings were applied |
| `ErrorCode` | A `MidiNetworkClientUpdateErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a translated description you can show to people.

Updating a client the service doesn't have returns `ClientNotFound`, not success.

`Success` means the service accepted and applied the settings, not that every one of them changed something you can see. A setting that only takes effect on the next connection, or that the remote device's function blocks override, still reports success. [MidiNetworkClientUpdateConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientUpdateConfig/) says which is which.
