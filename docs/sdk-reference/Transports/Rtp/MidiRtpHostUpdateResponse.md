---
layout: sdk_reference_page
title: MidiRtpHostUpdateResponse
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Result of a request to stop or start an RTP-MIDI host
---

Returned by `MidiRtpTransportManager.StopRtpHostAsync` and `StartRtpHostAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request was about |
| `Success` | True if the service took the request |
| `ErrorCode` | A `MidiRtpHostUpdateErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a description you can show to people.

A host starts in the background, so `Success` from `StartRtpHostAsync` doesn't mean it's running yet. Check `HasStarted` on its `MidiRtpConfiguredHost`.

Stopping and starting only change the running service. A saved host starts again the next time the service starts, even if it was stopped.
