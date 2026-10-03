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
| `Pending` | `0` | Set up, and waiting for the service to connect it. An entry also comes back here when the host says it's busy, and a direct connection by host name waits here between tries |
| `Active` | `1` | The service has created the client. Use `IsSessionActive` to see whether MIDI is flowing |
| `Failed` | `2` | The configuration entry was rejected, or the remote host turned the connection down, so trying again can't help |
| `Unavailable` | `3` | A direct connection by IP address that stopped answering, or that nobody gave permission to in time. The service won't try it again on its own |

## Remarks

`Unavailable` only applies to direct connections by IP address. Nothing announces that a fixed address is back, so the service stops sending it invitations, instead of sending network traffic forever. Call `ConnectNetworkClientAsync` again with the same `ClientId` to retry.

A discovered (mDNS) client never becomes `Unavailable`. It goes back to `Pending`, and it's picked up again whenever the host advertises.

A direct connection by host name never becomes `Unavailable` either. The service looks the name up again on every try, so it goes back to `Pending` and is tried again after `DirectConnectionScanIntervalMilliseconds` in [MidiNetworkTransportSettings]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportSettings/).

`Failed` is also what you see when the remote host turns the connection down: its owner refused it, or it wants authentication, which isn't built yet. Asking again won't change either one, so the service doesn't. Call `ConnectNetworkClientAsync` again with the same `ClientId` once something has changed.

A host that says it's busy hasn't turned the connection down. A Windows host also says this while it still holds this PC's previous session, which it lets go after about 10 seconds without an answer. That happens when a connection drops, for example because either PC's network address changed, and this PC reconnects first. So the entry goes back to `Pending`, and the service tries again after 10 to 30 seconds.
