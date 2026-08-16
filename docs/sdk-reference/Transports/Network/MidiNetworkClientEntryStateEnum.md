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
| `Pending` | `0` | Configured, and waiting for the service to connect it |
| `Active` | `1` | The service has created the client. Use `IsSessionActive` for whether MIDI is flowing |
| `Failed` | `2` | The configuration entry itself was rejected, so retrying cannot help |
| `Unavailable` | `3` | A direct connection which stopped answering. The service will not retry it on its own |

## Remarks

`Unavailable` applies only to direct address connections. Nothing announces that a fixed address has come back, so the service stops inviting it rather than putting traffic on the wire indefinitely. Call `ConnectNetworkClientAsync` again with the same `ClientId` to retry.

A discovered (mDNS) client never reaches `Unavailable`: it returns to `Pending` and is picked up again whenever the host advertises.
