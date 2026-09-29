---
layout: sdk_reference_page
title: MidiRtpClientDisconnectResponse
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Result of a request to disconnect and remove an RTP-MIDI client
---

Returned by `MidiRtpTransportManager.DisconnectRtpClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry the request was about |
| `Success` | True if the client was removed from the running service |
| `ErrorCode` | A `MidiRtpClientDisconnectErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a description you can show to people.
