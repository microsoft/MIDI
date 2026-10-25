---
layout: sdk_reference_page
title: MidiRtpRemoteClientSettings
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: The settings an RTP-MIDI host uses for one remote device instead of its own
---

Held in [MidiRtpHostRemoteClientSettingsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostRemoteClientSettingsConfig/).`RemoteClientSettings`. Reported by [MidiRtpConfiguredHost]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConfiguredHost/).`RemoteClientSettings` and [MidiRtpSavedHost]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSavedHost/).`RemoteClientSettings`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpRemoteClientSettings()` | Creates an empty entry with the default settings |
| `MidiRtpRemoteClientSettings(remoteClientName)` | Creates an entry for this remote device with the default settings |

## Properties

| Property | Description |
| -------- | ----------- |
| `RemoteClientName` | The name the remote device sends |
| `SendSpeedLimit` | How fast the host sends to this remote device. `Unlimited` by default. See [MidiRtpSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSendSpeedLimitEnum/) |
| `SkipInBetweenValuesToCatchUp` | True to skip some in-between steps of long control change, pressure and pitch bend moves when messages pile up behind `SendSpeedLimit`, so this device catches up. True by default. Set it to false for a device that needs every message, however late. Added in version 2 of `MidiTransportsRtpApiContract` |
| `OfflineRetentionSeconds` | How long the host keeps this device's endpoint and its MIDI 1.0 port after the device leaves. `Immediate` by default. See [MidiRtpOfflineRetention]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpOfflineRetentionEnum/). Added in version 2 of `MidiTransportsRtpApiContract` |
| `EndpointDeviceId` | Read only. The endpoint the host is keeping for this device while it's away. Empty while the device is connected, and when nothing is kept. Only filled in on the entries from `MidiRtpConfiguredHost.RemoteClientSettings`. It's never sent or saved. Added in version 2 of `MidiTransportsRtpApiContract` |

## Remarks

Use this when one device connected to a host needs settings different from the others. A hardware synth that loses data during a long SysEx dump can get MIDI 1.0 wire speed, while a computer connected to the same host gets no limit. For this device, `SendSpeedLimit` and `SkipInBetweenValuesToCatchUp` are used instead of the host's.

A remote device is known by the name it sends, ignoring uppercase and lowercase differences, the same way the host's remembered decisions know it. RTP-MIDI carries nothing else that stays the same from one connection to the next. An entry with no name is left out when the configuration is sent or saved.

The host uses this speed from the moment the device connects. A device that's already connected changes speed right away, without being disconnected.

RTP-MIDI can't tell this PC when a device misses data, so unlike Network MIDI 2.0 there's no choice to slow down by itself.

### Keeping the device's endpoint while it's away

Some people want Windows to be the host, so their device connects to this PC. `OfflineRetentionSeconds` lets the host keep that device's endpoint while it's switched off, the same way a connection this PC makes can. Apps keep their MIDI port, and anything they send to it while the device is away is thrown away. When the device connects again, it gets the same endpoint back.

- `KeepAlways` makes the endpoint as soon as the host starts, even before the device has ever connected. It's named after `RemoteClientName`.
- A number of seconds keeps the endpoint for that long after the device leaves. The time counts from when the device left, so a shorter time set while it's away can remove the endpoint at once.
- The setting is read when the device leaves, so you can change it while the device is connected.
- Changing it to `Immediate`, or removing the device's entry so it uses the host's settings, removes a kept endpoint straight away. So does turning off or removing the host.
- Disconnecting the device from this PC removes its endpoint when the time is a number of seconds. With `KeepAlways`, the endpoint stays.
- Only one device per name can keep its endpoint. If a second device sends a name that's already connected, it gets an endpoint of its own, and that one is removed when it leaves.

While the device is away, the kept endpoint only goes back to a device connecting from an address that device used before. Another device sending the same name from a different address is listed with `GetPendingRemoteClients()` and waits until someone approves it, even on a host that lets anyone in. Once approved, it gets the kept endpoint too. After the service restarts, a `KeepAlways` endpoint goes to the first device that connects with its name. So after the device's address changes, for example when a router hands it a new one, approve it once more.

A remote device is known by the name it sends, ignoring uppercase and lowercase differences, the same way the host's remembered decisions know it. RTP-MIDI carries nothing else that stays the same from one connection to the next. An entry with no name is left out when the configuration is sent or saved.

The host uses this speed from the moment the device connects. A device that's already connected changes speed right away, without being disconnected.

RTP-MIDI can't tell this PC when a device misses data, so unlike Network MIDI 2.0 there's no choice to slow down by itself.
