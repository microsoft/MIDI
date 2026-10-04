---
layout: sdk_reference_page
title: MidiRtpRemoteClientSettings
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: The sending speed an RTP-MIDI host uses for one remote device instead of its own
---

Held in [MidiRtpHostRemoteClientSettingsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostRemoteClientSettingsConfig/).`RemoteClientSettings`. Reported by [MidiRtpConfiguredHost]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConfiguredHost/).`RemoteClientSettings` and [MidiRtpSavedHost]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSavedHost/).`RemoteClientSettings`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpRemoteClientSettings()` | Creates an empty entry with no limit |
| `MidiRtpRemoteClientSettings(remoteClientName)` | Creates an entry for this remote device with no limit |

## Properties

| Property | Description |
| -------- | ----------- |
| `RemoteClientName` | The name the remote device sends |
| `SendSpeedLimit` | How fast the host sends to this remote device. `Unlimited` by default. See [MidiRtpSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSendSpeedLimitEnum/) |

## Remarks

Use this when one device connected to a host needs a different speed from the others. A hardware synth that loses data during a long SysEx dump can get MIDI 1.0 wire speed, while a computer connected to the same host gets no limit. For this device, `SendSpeedLimit` is used instead of the host's.

A remote device is known by the name it sends, ignoring uppercase and lowercase differences, the same way the host's remembered decisions know it. RTP-MIDI carries nothing else that stays the same from one connection to the next. An entry with no name is left out when the configuration is sent or saved.

The host uses this speed from the moment the device connects. A device that's already connected changes speed right away, without being disconnected.

RTP-MIDI can't tell this PC when a device misses data, so unlike Network MIDI 2.0 there's no choice to slow down by itself.
