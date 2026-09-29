---
layout: sdk_reference_page
title: MidiRtpPendingRemoteClient
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: A remote RTP-MIDI device waiting for a decision before it may connect
---

Returned by `MidiRtpTransportManager.GetPendingRemoteClients()`. Each entry is a remote device that asked to connect to one of this PC's hosts, where that host requires approval.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the remote device is trying to connect to |
| `HostServiceInstanceName` | The name that host is set up to advertise |
| `RemoteClientName` | The name the remote device sent. Show it to people, and pass it to `MidiRtpRemoteClientApprovalConfig` exactly as it is |
| `RemoteAddress` | Where the last request came from. For display only. It can change from one request to the next |
| `RequestTime` | When the remote device first asked |
| `IsApproved` | True when it's been approved for one connection, and the service is waiting for it to ask again |

## Remarks

RTP-MIDI has no way to tell a device to wait, so a remote device that's waiting gets no answer. It keeps asking for about twelve seconds, and then gives up. It stays in this list for two minutes after it last asked, so a decision made in that time still counts the next time it tries.

A host keeps at most 16 remote devices waiting. When that many are already waiting, or a remote device sends no name or a name longer than 255 characters, it's turned down right away instead of being listed.

No endpoint is created for a remote device that's waiting, so waiting costs almost nothing.

Approve or deny it with `MidiRtpTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`.
