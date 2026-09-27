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
| `HostId` | The GUID of the host the request was about |
| `Success` | True if the host was created and started |
| `ErrorCode` | A `MidiNetworkHostCreationErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a translated description you can show to people.

Because `CreateNetworkHostAsync` waits for the host to start, a `Success` of true means the host is listening and, if you asked for it, advertising. If the service accepted the host but it didn't start in time, `ErrorCode` is `TimedOutWaitingForHostToStart`.