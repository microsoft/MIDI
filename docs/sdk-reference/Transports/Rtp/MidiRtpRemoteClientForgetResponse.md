---
layout: sdk_reference_page
title: MidiRtpRemoteClientForgetResponse
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Result of dropping a remembered decision for one remote RTP-MIDI device
---

Returned by `MidiRtpTransportManager.ForgetRemoteClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request was about |
| `RemoteClientName` | The name of the remote device the request was about |
| `Success` | True if the host no longer has a decision for that remote device |
| `ErrorCode` | A `MidiRtpRemoteClientForgetErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Forgetting a remote device the host has no decision for reports success, because what you asked for is already true.
