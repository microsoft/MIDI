---
layout: sdk_reference_page
title: MidiNetworkHostRemovalResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of a request to remove a Network MIDI 2.0 host
---

Returned by `MidiNetworkTransportManager.RemoveNetworkHostAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request was about |
| `Success` | True if the host was removed |
| `ErrorCode` | A `MidiNetworkHostRemovalErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a translated description you can show to people.
