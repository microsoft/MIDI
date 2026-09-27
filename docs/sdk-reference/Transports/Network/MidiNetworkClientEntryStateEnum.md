---
layout: sdk_reference_page
title: MidiNetworkClientEntryState
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Where a configured Network MIDI 2.0 client entry is in its life
---

Reported by `MidiNetworkConfiguredClient.EntryState`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Pending` | `0` | Set up, and waiting for the service to connect it |
| `Active` | `1` | The service has created the client. Use `IsSessionActive` to see whether MIDI is flowing |
| `Failed` | `2` | The configuration entry itself was rejected, so trying again can't help |
| `Unavailable` | `3` | A direct connection that stopped answering. The service won't try it again on its own |

## Remarks

`Unavailable` only applies to direct address connections. Nothing announces that a fixed address is back, so the service stops sending it invitations, instead of sending network traffic forever. Call `ConnectNetworkClientAsync` again with the same `ClientId` to retry.

A discovered (mDNS) client never becomes `Unavailable`. It goes back to `Pending`, and it's picked up again whenever the host advertises.
