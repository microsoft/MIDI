---
layout: sdk_reference_page
title: MidiRtpOfflineRetention
namespace: Windows.Devices.Midi2.Transports.Rtp
type: enum
description: How long the MIDI endpoint for an RTP-MIDI connection stays while the remote device is away
---

Controls what happens to the MIDI endpoint for an RTP-MIDI connection when the remote device goes away. That's when it's switched off, loses its network connection, or stops answering. It applies to connections this PC makes, and to devices that connect to a host on this PC and have settings of their own there.

Any value greater than zero is a number of seconds, so this enumeration only names the values that aren't an amount of time. Because of that, the API takes and returns a plain `Int32`, not this type. Use these named values for the special cases, and a number from 1 to 86400 for a delay.

Added in version 2 of `MidiTransportsRtpApiContract`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `KeepAlways` | `-1` | The endpoint and its MIDI 1.0 port stay until the entry is removed, turned off or disconnected. The endpoint is there as soon as the MIDI service starts, even before the remote device answers |
| `Immediate` | `0` | The endpoint and its MIDI 1.0 port are removed as soon as the connection ends, and created again when it comes back. This is the default |

## Why this is a setting

Many apps, DAWs especially, remember their setup by port. If a device is switched off when the app starts, its port is missing, and some apps drop the device from their setup. Keeping the endpoint fixes that. The port is always there, and it starts working when the device answers.

It's off by default because it has a cost. Messages sent while the device is away are thrown away, and the app isn't told. An app written for WinMM or WinRT MIDI 1.0 can't ask Windows whether a device is present, so whether the port exists is the only clue it gets.

## Remarks

Only an entry with `AutoReconnect` turned on keeps its endpoint, because only those come back by themselves. A device that connects to a host on this PC keeps its endpoint only when the host has settings for it with this set, in [MidiRtpRemoteClientSettings]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpRemoteClientSettings/). Every other device that connects to a host has its endpoint removed when it leaves.

With a number of seconds, the endpoint stays for that long after the connection ends, then it's removed. It's created again when the device comes back. The time counts from the connection ending, so a device that's off when the MIDI service starts has no endpoint until it first connects.

The endpoint is removed and created again when the entry's name, or the remote device it points at, changes. Changing only this setting doesn't disconnect anything.

For a connection this PC makes, set it with `MidiRtpClientConnectConfig.OfflineRetentionSeconds`, and read it from `MidiRtpConfiguredClient.OfflineRetentionSeconds` and `MidiRtpSavedClient.OfflineRetentionSeconds`. While the device is away, `MidiRtpConfiguredClient.EndpointDeviceId` still gives the endpoint's id.

For a device that connects to a host on this PC, set it with `MidiRtpRemoteClientSettings.OfflineRetentionSeconds`. While the device is away, `MidiRtpRemoteClientSettings.EndpointDeviceId` in `MidiRtpConfiguredHost.RemoteClientSettings` gives the endpoint's id.
