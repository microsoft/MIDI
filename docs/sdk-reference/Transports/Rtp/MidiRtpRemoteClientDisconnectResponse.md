---
layout: sdk_reference_page
title: MidiRtpRemoteClientDisconnectResponse
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Result of ending one connection to an RTP-MIDI host on this PC
---

Returned by `MidiRtpTransportManager.DisconnectRemoteClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request was about |
| `Success` | True if the connection was ended |
| `ErrorCode` | A `MidiRtpRemoteClientDisconnectErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

`ConnectionNotFound` most often means the remote device had already left by the time someone asked to disconnect it.
