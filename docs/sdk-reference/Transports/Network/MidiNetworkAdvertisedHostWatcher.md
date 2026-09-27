---
layout: sdk_reference_page
title: MidiNetworkAdvertisedHostWatcher
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Watches the network for Network MIDI 2.0 hosts appearing and disappearing
---

A device watcher made just for Network MIDI 2.0 hosts advertised over mDNS.

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `Create()` | Creates a watcher. Add your event handlers before you call `Start()` |

## Properties

| Property | Description |
| -------- | ----------- |
| `EnumeratedHosts` | A map of device id to `MidiNetworkAdvertisedHost` for everything found so far |
| `IsStarted` | True while the watcher is running |

## Methods

| Method | Description |
| -------- | ----------- |
| `Start()` | Starts watching |
| `Stop()` | Stops watching |

## Events

| Event | Description |
| -------- | ----------- |
| `Added` | A host was found. The args are `MidiNetworkAdvertisedHostAddedEventArgs` |
| `Removed` | A host went away. The args are `MidiNetworkAdvertisedHostRemovedEventArgs` |
| `Updated` | An advertised host's properties changed. The args are `MidiNetworkAdvertisedHostUpdatedEventArgs` |
| `EnumerationCompleted` | The first search finished. Hosts may still be added after this |
| `Stopped` | The watcher stopped |

## Remarks

`EnumerationCompleted` means the first search is done, not that discovery is finished. Devices can show up at any time, so keep the watcher running, instead of treating that event as the final list. mDNS discovery can take a while to find everything.