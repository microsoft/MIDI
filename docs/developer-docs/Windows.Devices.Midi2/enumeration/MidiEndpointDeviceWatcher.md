---
layout: api_page
title: MidiEndpointDeviceWatcher
parent: Endpoint Enumeration
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiEndpointDeviceWatcher

WinRT provides a `Windows.Devices.Enumeration` namespace with a `DeviceWatcher` class. That class is generic to any type of device, and so requires additional work to use with MIDI devices. Because of that, we've wrapped that functionality in the `MidiEndpointDeviceWatcher` class and the related `MidiEndpointDeviceInformation` class.

This is the class applications should use when they want to find devices, and also be notified when devices are added or removed, or when properties like function blocks or device names change.

Create a `MidiEndpointDeviceWatcher` on a background thread, and use the internal list of Endpoints as your source of record for device properties.

## Properties

| Function | Description |
| --------------- | ----------- |
| `Status` | The current status. See the `Windows.Devices.Enumeration.DeviceWatcherStatus` enumeration |
| `EnumeratedEndpointDevices` | The list of enumerated devices. Provided here for convenience so applications do not need to keep their own list of MIDI devices.  |

## Functions

| Function | Description |
| --------------- | ----------- |
| `Start()` | Begin device enumeration. Wire up event handlers before calling this function.  |
| `Stop()` | Stop device enumeration. |


## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| `CreateWatcher(endpointFilter)` | Create a watcher which will enumerate devices based on the provided filter |

## Events

| Event | Description |
| --------------- | ----------- |
| `Added(source, deviceInformation)` | A new endpoint has been added.  |
| `Removed(source, deviceInformationUpdate)` | An endpoint has been removed. |
| `Updated(source endpointDeviceInformationUpdate)` | Properties of an endpoint have been updated. This is much more common than it was with the older MIDI 1.0 APIs due to both in-protocol endpoint information, and user configuration. |
| `EnumerationCompleted(source)` | Raised when the initial device enumeration has been completed. Devices may still be added or removed after this event, but use this to decide when you have enough information to display an initial list. |
| `Stopped(source)` | Enumeration has been stopped. |

## IDL

[MidiEndpointDeviceWatcher IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiEndpointDeviceWatcher.idl)
