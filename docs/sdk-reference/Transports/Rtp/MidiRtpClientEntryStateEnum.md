---
layout: sdk_reference_page
title: MidiRtpClientEntryState
namespace: Windows.Devices.Midi2.Transports.Rtp
type: enum
description: Where a configured RTP-MIDI client entry is in its life
---

Reported by `MidiRtpConfiguredClient.EntryState`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Pending` | `0` | Set up, and waiting to find or reach the remote device |
| `Active` | `1` | Connected |
| `Retrying` | `2` | The last try didn't work, or the connection ended, and the service will try again by itself. `LastErrorCode` says why |
| `Unavailable` | `3` | The service won't try again until `ReconnectRtpClientAsync` is called. This happens when `AutoReconnect` is false and a try or a connection ends, or when the connection was ended from this PC |

## Remarks

A remote device that can't be found yet keeps the entry at `Pending`, whether `AutoReconnect` is on or not, because the service hasn't been able to try. See the [namespace overview]({{ site.baseurl }}/sdk-reference/Transports/Rtp/) for when the service tries again.
