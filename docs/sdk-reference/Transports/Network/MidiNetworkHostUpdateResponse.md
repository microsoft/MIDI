---
layout: sdk_reference_page
title: MidiNetworkHostUpdateResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of a request to start or stop a Network MIDI 2.0 host
---

Returned by `MidiNetworkTransportManager.StartNetworkHostAsync` and `StopNetworkHostAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request referred to |
| `Success` | True if the host was started or stopped as requested |
| `ErrorCode` | A `MidiNetworkHostUpdateErrorCode` when `Success` is false |
| `ErrorMessage` | A human-readable description of the failure |

## Remarks

Check `Success` first. When it is false, `ErrorCode` gives the machine-readable reason and `ErrorMessage` a localized description suitable for display.
