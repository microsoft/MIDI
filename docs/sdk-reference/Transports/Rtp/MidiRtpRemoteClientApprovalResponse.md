---
layout: sdk_reference_page
title: MidiRtpRemoteClientApprovalResponse
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Result of approving or denying a remote RTP-MIDI device
---

Returned by `MidiRtpTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the decision was for |
| `RemoteClientName` | The name of the remote device the decision was for |
| `Success` | True if the decision was applied |
| `ErrorCode` | A `MidiRtpRemoteClientApprovalErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

A decision for one request fails with `PendingRemoteClientNotFound` when the remote device isn't waiting any more. That's normal if it stopped asking a couple of minutes before someone decided. A decision that isn't limited to one request still works then.
