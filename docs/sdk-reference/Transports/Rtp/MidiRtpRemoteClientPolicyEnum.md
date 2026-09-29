---
layout: sdk_reference_page
title: MidiRtpRemoteClientPolicy
namespace: Windows.Devices.Midi2.Transports.Rtp
type: enum
description: What an RTP-MIDI host does when a remote device it hasn't been told about asks to connect
---

Used by `MidiRtpHostCreationConfig.RemoteClientPolicy`, and reported by `MidiRtpConfiguredHost.RemoteClientPolicy`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `AllowAny` | `0` | Remote devices are let in, unless they've been denied |
| `RequireApproval` | `1` | Remote devices wait until they're approved, unless they've already been allowed. Denied devices are turned away |

## Remarks

RTP-MIDI has no way to tell a device to wait, so a remote device that needs approval gets no answer. It keeps asking for about twelve seconds, and gets in on its first request after it's approved. See `MidiRtpTransportManager.GetPendingRemoteClients()`.
