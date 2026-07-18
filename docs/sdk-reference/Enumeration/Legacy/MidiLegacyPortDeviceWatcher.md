---
layout: sdk_reference_page
title: MidiLegacyPortDeviceWatcher
namespace: Windows.Devices.Midi2.Enumeration.Legacy
type: runtimeclass
description: Watcher for MIDI 1.0 legacy port devices
---

This class is the legacy equivalent of `MidiEndpointDeviceWatcher`, but for MIDI 1.0 ports created by Windows MIDI Services.

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `Create()` | Create a watcher for all MIDI 1.0 ports |
| `Create(flow)` | Create a watcher for MIDI 1.0 ports in the specified direction |

## Methods

| Method | Description |
| ------ | ----------- |
| `Start()` | Begin enumeration. Wire up event handlers before calling this. |
| `Stop()` | Stop enumeration. |

## Properties

| Property | Description |
| -------- | ----------- |
| `Status` | The current `DeviceWatcherStatus` |
| `EnumeratedEndpointDevices` | Map of all currently enumerated MIDI 1.0 ports, keyed by port device id |

## Events

| Event | Description |
| ----- | ----------- |
| `Added(source, args)` | A new MIDI 1.0 port has been added |
| `Removed(source, args)` | A MIDI 1.0 port has been removed |
| `Updated(source, args)` | A MIDI 1.0 port has been updated |
| `EnumerationCompleted(source)` | Initial enumeration has completed |
| `Stopped(source)` | The watcher has been stopped |
