---
layout: sdk_reference_page
title: MidiNetworkAdvertisedHostRemovedEventArgs
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Event args for a Network MIDI 2.0 host disappearing from the network
---

Supplied by `MidiNetworkAdvertisedHostWatcher.Removed`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostDeviceId` | The device id of the host which went away |
| `FullName` | The full DNS-SD name, such as `instance._midi2._udp.local`, for logging and for matching against a stored entry |