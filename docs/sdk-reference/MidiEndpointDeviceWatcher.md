---
layout: sdk_reference_page
title: MidiEndpointDeviceWatcher
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Recommended class to use when enumerating endpoints
---

WinRT provides a `Windows.Devices.Enumeration` namespace with a `DeviceWatcher` class. That class is generic to any type of device, and so requires additional work to use with MIDI devices. Because of that, we've wrapped that functionality in the `MidiEndpointDeviceWatcher` class and the related `MidiEndpointDeviceInformation` class.

This is the class applications should use when they want to find devices, and also be notified when devices are added or removed, or when properties like function blocks or device names change.

Create a `MidiEndpointDeviceWatcher` on a background thread, and use the internal list of Endpoints as your source of record for device properties.

## Properties

| Function | Description |
| --------------- | ----------- |
| `Status` | The current status of the watcher itself. See the `Windows.Devices.Enumeration.DeviceWatcherStatus` enumeration |
| `EnumeratedEndpointDevices` | The list of enumerated devices. Provided here for convenience so applications do not need to keep their own list of MIDI devices. The key into this map is the full MIDI Endpoint Device Id.  |

## Functions

| Function | Description |
| --------------- | ----------- |
| `Start()` | Begin device enumeration. Wire up event handlers before calling this function.  |
| `Stop()` | Stop device enumeration. |

## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| `Create (endpointFilter)` | Create a watcher which will enumerate devices based on the provided filter |
| `Create ()` | Create a watcher which will enumerate devices based on the default filter, appropriate for most apps |

## Events

| Event | Description |
| --------------- | ----------- |
| `Added (source, deviceInformationAddedEventArgs)` | A new endpoint has been added.  |
| `Removed (source, deviceInformationRemovedEventArgs)` | An endpoint has been removed. |
| `Updated (source deviceInformationUpdatedEventArgs)` | Properties of an endpoint have been updated. This is much more common than it was with the older MIDI 1.0 APIs due to both in-protocol endpoint information, and user configuration. |
| `EnumerationCompleted (source)` | Raised when the initial device enumeration has been completed. Devices may still be added or removed after this event, but use this to decide when you have enough information to display an initial list. |
| `Stopped (source)` | Enumeration has been stopped. |

## Endpoint connectivity detection

Whether and when an event fires depends upon the underlying transport. In the case of USB, in the service, we have a lower-level device watcher checking for the USB disconnects. When we receive one, we add/remove/update the software devices as appropriate. This, in turn, causes these events to fire in the API.

**All Windows MIDI Services endpoints are enumerated and tied to the lifetime of the Windows service.** Shutting down the Windows service will cause all enumerated devices to disconnect and raise the watcher removed events.

### USB

When the USB stack disconnects a device, we see it as a device removal event. This can happen when the USB device is physically disconnected, when it is powered down, or when the PC is put into suspend and disconnects all USB devices. When the PC wakes from suspend, and the USB stack notifies us that the device is reconnected, it reappears and you can then reconnect to it. 

### Bluetooth MIDI

The Blueooth MIDI stack has more of a delay, depending upon how often the device is checked for connectivity. It may be a few seconds before a powered-down BLE MIDI device triggers a `Remove` event. 

### Virtual Device (app to app) MIDI

In the case of the virtual device MIDI feature, when the hosting application closes the device-side connection, the device is removed and the appropriate event fires. 

### Loopback (app to app) MIDI

Loopback endpoints (other than the two diagnostic loopbacks built-in for testing/development) are controlled through either the API or the configuration file. Those created through the configuration file never go away, so there is no `Removed` event fired. Those created through the API can be created/removed through the API at any time, and so will have the appropriate events fire off.

### Network MIDI 2.0

When the connection is lost or explicitly closed, the endpoint device will be removed and the `Removed` event will fire.

## What happens when an endpoint is disconnected

When an endpoint is disconnected due to the parent device going away, all client connections are closed, and the device connection in the service is also closed. The cross-process message queues are torn down, and any messages there, in the outbound scheduler, or otherwise in the service pipelines are lost.

If the auto-reconnect option was used when creating the connection from the SDK, a watcher is set up behind the scenes, and the endpoint is reconnected when it comes back online, assuming its id has not changed.
