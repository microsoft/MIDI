---
layout: sdk_reference_page
title: MidiRtpHostCreationResponse
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Result of a request to create an RTP-MIDI host
---

Returned by `MidiRtpTransportManager.CreateRtpHostAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request was about |
| `Success` | True if the host was created and started |
| `ErrorCode` | A `MidiRtpHostCreationErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a description you can show to people.

Because `CreateRtpHostAsync` waits for the host to start, a `Success` of true means other devices can connect now. A host that couldn't be advertised still starts, because it can be reached by its address. In that case, `LastErrorCode` on its `MidiRtpConfiguredHost` says why advertising didn't work.

If the service accepted the host but it didn't start within about ten seconds, `ErrorCode` is `TimedOutWaitingForHostToStart`. The host isn't removed. The service tries to start it again every 15 seconds, and `LastErrorCode` on its `MidiRtpConfiguredHost` says what went wrong.
