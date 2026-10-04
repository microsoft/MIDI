---
layout: sdk_reference_page
title: MidiNetworkRemoteClientSettings
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: The sending speed a Network MIDI 2.0 host uses for one remote client instead of its own
---

Held in [MidiNetworkHostRemoteClientSettingsConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostRemoteClientSettingsConfig/).`RemoteClientSettings`. Reported by [MidiNetworkConfiguredHost]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkConfiguredHost/).`RemoteClientSettings` and [MidiNetworkSavedHost]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSavedHost/).`RemoteClientSettings`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkRemoteClientSettings()` | Creates an empty entry with no limit |
| `MidiNetworkRemoteClientSettings(remoteClientName, remoteClientProductInstanceId)` | Creates an entry for this client with no limit |

## Properties

| Property | Description |
| -------- | ----------- |
| `RemoteClientName` | The UMP endpoint name the remote client announces |
| `RemoteClientProductInstanceId` | The product instance id the remote client announces |
| `SendSpeedLimit` | How fast the host sends to this client. `Unlimited` by default. See [MidiNetworkSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSendSpeedLimitEnum/) |
| `ReduceSendSpeedAutomatically` | True to send more slowly while this client keeps asking for data again, down to MIDI 1.0 wire speed, and go back up to `SendSpeedLimit` once it stops. False by default |

## Remarks

Use this when one device connected to a host needs a different speed from the others. A hardware synth that loses data during a long SysEx dump can get MIDI 1.0 wire speed, while a computer connected to the same host gets no limit. For this client, these settings are used instead of the host's `SendSpeedLimit` and `ReduceSendSpeedAutomatically`.

A remote client is recognized by its name and product instance id together, ignoring uppercase and lowercase differences, the same way the host's allow and deny lists recognize it. Both are needed. An entry missing either one is left out when the configuration is sent or saved, because the service could never match it to a client.

The host uses these settings from the moment the client connects. A client that's already connected changes speed right away, without being disconnected.
