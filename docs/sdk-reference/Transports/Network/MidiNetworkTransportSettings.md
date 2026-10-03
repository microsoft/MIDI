---
layout: sdk_reference_page
title: MidiNetworkTransportSettings
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: The settings which apply to the Network MIDI 2.0 transport as a whole, rather than to any one host or client
---

Read the current values with [MidiNetworkTransportManager]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportManager/).`GetTransportSettings()`, change what you need, and then send the object back to apply it.

This class implements [IMidiServiceTransportPluginConfig]({{ site.baseurl }}/sdk-reference/ServiceConfig/IMidiServiceTransportPluginConfig/). Pass the same object to `MidiServiceTransportPluginConfigManager.SendUpdate` to apply it now, or to `SaveUpdate` to also keep it after the service restarts.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiNetworkTransportSettings()` | Creates settings with every property at its default. Sending it puts every setting back to its default, so start from `GetTransportSettings()` instead |

## Properties

| Property | Default | Range | When a change takes effect |
| -------- | ------- | ----- | -------------------------- |
| `MaxForwardErrorCorrectionCommandPackets` | 2 | 0 – 10 | Read when a connection is created, so existing sessions pick it up only when they reconnect |
| `MaxRetransmitBufferCommandPackets` | 50 | 0 – 1000 | Read when a connection is created |
| `OutboundPingIntervalMilliseconds` | 2000 | 250 – 120000 | Read on each pass of the connection watcher, so it reaches open sessions within one interval |
| `InvitationPendingTimeoutMilliseconds` | 120000 | 1000 – 600000 | Applies to invitations from that point on |
| `MaxHostConnections` | 64 | 1 – 512 | Immediately. Checked as each invitation arrives |
| `DirectConnectionScanIntervalMilliseconds` | 20000 | 250 – 300000 | Read at the top of each scan |

## Static Properties

Every property above has a matching pair of static properties with its supported range. A settings screen can use them, instead of hard-coding numbers that may change.

| Static Property | Description |
| --------------- | ----------- |
| `MinMaxForwardErrorCorrectionCommandPackets` / `MaxMaxForwardErrorCorrectionCommandPackets` | Range for `MaxForwardErrorCorrectionCommandPackets` |
| `MinMaxRetransmitBufferCommandPackets` / `MaxMaxRetransmitBufferCommandPackets` | Range for `MaxRetransmitBufferCommandPackets` |
| `MinOutboundPingIntervalMilliseconds` / `MaxOutboundPingIntervalMilliseconds` | Range for `OutboundPingIntervalMilliseconds` |
| `MinInvitationPendingTimeoutMilliseconds` / `MaxInvitationPendingTimeoutMilliseconds` | Range for `InvitationPendingTimeoutMilliseconds` |
| `MinMaxHostConnections` / `MaxMaxHostConnections` | Range for `MaxHostConnections` |
| `MinDirectConnectionScanIntervalMilliseconds` / `MaxDirectConnectionScanIntervalMilliseconds` | Range for `DirectConnectionScanIntervalMilliseconds` |

## Remarks

**Values out of range are adjusted, not refused.** A value outside the supported range is changed to the nearest limit, and a missing value or one of the wrong type becomes the default. So a setting always ends up with a value that works. Read the object afterward to see what was really used.

**Sending only some of the settings resets the rest.** Each time, the service starts from the defaults and reads this section, instead of merging it with what's already there. Always read the current settings with `GetTransportSettings()`, change the properties you care about, and send the whole object back. If you create a new `MidiNetworkTransportSettings` and set one property, everything else goes back to its default.

**What you read is what's really running.** `GetTransportSettings()` reports the values the transport is really using, after any corrections. If the configuration has a value that's out of range, this is how you see what it became.

Lowering `MaxHostConnections` doesn't disconnect clients that are already connected. It only affects invitations that arrive later.

`DirectConnectionScanIntervalMilliseconds` is how long the service waits before it tries a direct client again, after the host stopped answering. It's also the longest the service waits between looks at clients that are waiting to connect. See [Reconnection behavior]({{ site.baseurl }}/sdk-reference/Transports/Network/#reconnection-behavior).

## See also

- [MidiNetworkTransportManager]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportManager/)
- [MidiServiceTransportPluginConfigManager]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceTransportPluginConfigManager/)
- [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/)
