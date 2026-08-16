---
layout: sdk_reference_page
title: MidiNetworkHostCreationResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of a request to create a Network MIDI 2.0 host
---

Returned by `MidiNetworkTransportManager.CreateNetworkHostAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request referred to |
| `Success` | True if the host was created and started |
| `ErrorCode` | A `MidiNetworkHostCreationErrorCode` when `Success` is false |
| `ErrorMessage` | A human-readable description of the failure |

## Remarks

Check `Success` first. When it is false, `ErrorCode` gives the machine-readable reason and `ErrorMessage` a localized description suitable for display.

Because `CreateNetworkHostAsync` waits for the host to start, a `Success` of true means the host is listening and, if requested, advertising. If the definition was accepted but the host did not come up in time, `ErrorCode` is `TimedOutWaitingForHostToStart`.