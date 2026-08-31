---
layout: sdk_reference_page
title: MidiNetworkTransportSettings
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: The settings which apply to the Network MIDI 2.0 transport as a whole, rather than to any one host or client
---

Read the current values with [MidiNetworkTransportManager](MidiNetworkTransportManager).`GetTransportSettings()`, change what you need, then send the object back to apply it.

This class implements [IMidiServiceTransportPluginConfig]({{ site.baseurl }}/sdk-reference/ServiceConfig/IMidiServiceTransportPluginConfig), so the same object is passed to `MidiServiceTransportPluginConfigManager.SendUpdate` to apply it now, or `SaveUpdate` to also keep it across a service restart.

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

Every property above has a matching pair of statics giving its supported range, so a UI can bind to them rather than hard-coding numbers which may change.

| Static Property | Description |
| --------------- | ----------- |
| `MinMaxForwardErrorCorrectionCommandPackets` / `MaxMaxForwardErrorCorrectionCommandPackets` | Range for `MaxForwardErrorCorrectionCommandPackets` |
| `MinMaxRetransmitBufferCommandPackets` / `MaxMaxRetransmitBufferCommandPackets` | Range for `MaxRetransmitBufferCommandPackets` |
| `MinOutboundPingIntervalMilliseconds` / `MaxOutboundPingIntervalMilliseconds` | Range for `OutboundPingIntervalMilliseconds` |
| `MinInvitationPendingTimeoutMilliseconds` / `MaxInvitationPendingTimeoutMilliseconds` | Range for `InvitationPendingTimeoutMilliseconds` |
| `MinMaxHostConnections` / `MaxMaxHostConnections` | Range for `MaxHostConnections` |
| `MinDirectConnectionScanIntervalMilliseconds` / `MaxDirectConnectionScanIntervalMilliseconds` | Range for `DirectConnectionScanIntervalMilliseconds` |

## Remarks

**Values are clamped, not refused.** A value outside the supported range becomes the nearest bound, and a missing or wrong-typed value becomes the default. A setting therefore always ends up somewhere usable. Read the object back afterwards to see what was actually taken.

**Sending a partial set of settings resets the rest.** The service parses this section starting from the defaults each time rather than merging it into what is already there. Always read the current settings with `GetTransportSettings()`, change the properties you care about, and send the whole object back. Constructing a fresh `MidiNetworkTransportSettings` and setting one property will return everything else to its default.

**What you read back is what is running, not what is in the file.** `GetTransportSettings()` reports the values the transport is actually using, after any correction. If a hand-edited configuration file contains an out-of-range value, this is how you see what it became.

Lowering `MaxHostConnections` does not disconnect clients which are already connected. It only affects invitations which arrive afterwards.

## See also

- [MidiNetworkTransportManager](MidiNetworkTransportManager)
- [MidiServiceTransportPluginConfigManager]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceTransportPluginConfigManager)
- [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/)
