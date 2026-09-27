---
layout: sdk_reference_page
title: MidiNetworkAdvertisedHostUpdatedEventArgs
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Event args for a change to an advertised Network MIDI 2.0 host
---

Supplied by `MidiNetworkAdvertisedHostWatcher.Updated`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostDeviceId` | The device id of the host that changed |
| `ChangedProperties` | Which fields changed, as `MidiNetworkAdvertisedHostChangedProperties`. This event is only raised for a real change, never when a host just announces itself again, so this is never `None` |
| `UpdatedHost` | The host as it is now, with all of its properties |