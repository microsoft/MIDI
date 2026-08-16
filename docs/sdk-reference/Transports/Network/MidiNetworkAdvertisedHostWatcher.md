---
layout: sdk_reference_page
title: MidiNetworkAdvertisedHostWatcher
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Watches the network for Network MIDI 2.0 hosts appearing and disappearing
---

A device watcher specialized for Network MIDI 2.0 hosts advertised over mDNS.

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `Create()` | Create a watcher. Wire up the events before calling `Start()` |

## Properties

| Property | Description |
| -------- | ----------- |
| `EnumeratedHosts` | A map of device id to `MidiNetworkAdvertisedHost` for everything found so far |
| `Status` | The underlying `DeviceWatcherStatus` |

## Methods

| Method | Description |
| -------- | ----------- |
| `Start()` | Begin watching |
| `Stop()` | Stop watching |

## Events

| Event | Description |
| -------- | ----------- |
| `Added` | A host was discovered. Args are `MidiNetworkAdvertisedHostAddedEventArgs` |
| `Removed` | A host went away. Args are `MidiNetworkAdvertisedHostRemovedEventArgs` |
| `Updated` | An advertised host's properties changed. Args are `MidiNetworkAdvertisedHostUpdatedEventArgs` |
| `EnumerationCompleted` | The initial enumeration pass finished. Hosts may still be added afterwards |
| `Stopped` | The watcher stopped |

## Remarks

`EnumerationCompleted` means the first pass is done, not that discovery is finished. Devices can appear at any time, so keep the watcher running rather than treating that event as a final list. mDNS discovery can take a noticeable time to settle.