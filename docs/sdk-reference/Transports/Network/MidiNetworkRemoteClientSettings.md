---
layout: sdk_reference_page
title: MidiNetworkRemoteClientSettings
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: The sending speed and MIDI 1.0 ports a Network MIDI 2.0 host uses for one remote client instead of its own
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
| `SkipInBetweenValuesToCatchUp` | True to skip some in-between steps of long control change, pressure and pitch bend moves when messages pile up behind `SendSpeedLimit`, so this client catches up. True by default. Set it to false for a device that needs every message, however late. Added in version 2 of `MidiTransportsNetworkApiContract` |
| `FallbackMidi1PortGroups` | The groups to create MIDI 1.0 ports for when this client doesn't describe itself, and what to call them. A list of [MidiNetworkMidi1PortGroup]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkMidi1PortGroup/), empty by default. Used instead of the host's when it isn't empty. Added in version 2 of `MidiTransportsNetworkApiContract` |
| `UseHostSendSpeed` | True to ignore `SendSpeedLimit`, `ReduceSendSpeedAutomatically` and `SkipInBetweenValuesToCatchUp` here and send at the host's speed. For an entry that's only there for its `FallbackMidi1PortGroups`. False by default. Added in version 2 of `MidiTransportsNetworkApiContract` |

## Remarks

Use this when one device connected to a host needs a different speed from the others. A hardware synth that loses data during a long SysEx dump can get MIDI 1.0 wire speed, while a computer connected to the same host gets no limit. For this client, these settings are used instead of the host's `SendSpeedLimit`, `ReduceSendSpeedAutomatically` and `SkipInBetweenValuesToCatchUp`.

Use it also to name the MIDI 1.0 ports one device gets, when that device doesn't describe its own groups, such as another Windows PC. `FallbackMidi1PortGroups` is used for this client instead of the host's ports. When the entry is only there for the ports, set `UseHostSendSpeed` to true, so the client still gets the host's speed. A transport from before these were added ignores both, and uses this entry's speed.

A remote client is recognized by its name and product instance id together, ignoring uppercase and lowercase differences, the same way the host's allow and deny lists recognize it. Both are needed. An entry missing either one is left out when the configuration is sent or saved, because the service could never match it to a client.

The host uses these settings from the moment the client connects. A client that's already connected changes speed right away, without being disconnected.
