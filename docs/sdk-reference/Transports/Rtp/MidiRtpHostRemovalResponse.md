---
layout: sdk_reference_page
title: MidiRtpHostRemovalResponse
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Result of a request to remove an RTP-MIDI host
---

Returned by `MidiRtpTransportManager.RemoveRtpHostAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request was about |
| `Success` | True if the host was removed from the running service |
| `ErrorCode` | A `MidiRtpHostRemovalErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a description you can show to people.

`HostNotFound` means the running service has no host with that id. That's normal for a host that was created without being saved, after the service has restarted.
