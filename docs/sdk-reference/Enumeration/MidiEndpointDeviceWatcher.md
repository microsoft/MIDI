---
layout: sdk_reference_page
title: MidiEndpointDeviceWatcher
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Recommended class to use when enumerating endpoints
---

WinRT has a `DeviceWatcher` class in the `Windows.Devices.Enumeration` namespace. It works with every kind of device, so it takes extra work to use with MIDI devices. That's why we wrapped it in `MidiEndpointDeviceWatcher` and the related `MidiEndpointDeviceInformation` class.

Use this class to find devices, and to hear about it when devices are added or removed, or when properties such as function blocks or device names change.

Create a `MidiEndpointDeviceWatcher` on a background thread, and treat its list of endpoints as the true source of device properties.

## Properties

| Property | Description |
| --------------- | ----------- |
| `Status` | The watcher's own status. See the `Windows.Devices.Enumeration.DeviceWatcherStatus` enumeration |
| `EnumeratedEndpointDevices` | The endpoints the watcher has found. It's here so your application doesn't have to keep its own list of MIDI devices. The map key is the endpoint's full device id |

## Functions

| Function | Description |
| --------------- | ----------- |
| `Start()` | Starts finding devices. Attach your event handlers before you call this |
| `Stop()` | Stops finding devices |

## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| `Create(endpointFilters)` | Creates a watcher that finds the kinds of endpoints in the filter |
| `Create()` | Creates a watcher that uses the default filter, which is right for most applications |

## Events

The underlying `DeviceWatcher` raises these events and waits for your handlers, so the same rules and advice apply as for the WinRT `Windows.Devices.Enumeration.DeviceWatcher` type. We also recommend:

- Don't do slow work in an event handler.
- Don't create or remove virtual devices, loopbacks, or network connections in a handler, and don't change properties in a way that would cause more Plug and Play events.
- If you're not sure, pass the data to a worker thread and return from the handler quickly.

| Event | Description |
| --------------- | ----------- |
| `Added(source, deviceInformationAddedEventArgs)` | Raised when an endpoint is added |
| `Removed(source, deviceInformationRemovedEventArgs)` | Raised when an endpoint is removed |
| `Updated(source, deviceInformationUpdatedEventArgs)` | Raised when an endpoint's properties change. This happens much more often than with the older MIDI 1.0 APIs, because devices describe themselves in the protocol, and users can change settings |
| `EnumerationCompleted(source)` | Raised when the first pass of finding devices is done. Devices can still be added or removed after this, but use it to decide when you have enough to show a first list |
| `Stopped(source)` | Raised when the watcher stops |

## Detecting connects and disconnects

Whether and when an event fires depends on the transport. For USB, the service watches for USB devices being connected and disconnected. When that happens, it adds, removes, or updates the endpoint's software device, and that raises these events in the API.

**Every Windows MIDI Services endpoint lasts only as long as the MIDI service.** Stopping the service disconnects every endpoint and raises the watcher's `Removed` events.

### USB

When Windows disconnects a USB device, its endpoint is removed. That happens when the device is unplugged or switched off, or when the PC goes to sleep and disconnects all its USB devices. When the PC wakes up and Windows reports that the device is back, the endpoint comes back and you can connect to it again.

### Bluetooth MIDI

By default, a Bluetooth MIDI endpoint stays when its device sleeps, goes out of range, or is switched off, so no `Removed` event fires. When the device comes back, the endpoint starts working again. The customer can change this for one device or for all Bluetooth devices, so the endpoint is removed right away or after a delay. See [`MidiBluetoothOfflineRetention`]({{ site.baseurl }}/sdk-reference/Transports/Bluetooth/MidiBluetoothOfflineRetentionEnum/). When a Bluetooth device is disconnected on purpose, its endpoint is removed and `Removed` fires.

### Virtual device (app to app) MIDI

When the application that hosts a virtual device closes its device-side connection, the device is removed and `Removed` fires.

### Loopback (app to app) MIDI

Loopback endpoints, other than the two built-in diagnostic loopbacks, are either created through the API or saved in the MIDI configuration. Saved ones never go away, so they never raise `Removed`. Ones created through the API can be created and removed at any time, and they raise the matching events.

### Network MIDI 2.0

When the connection is lost or closed, the endpoint is removed and `Removed` fires.

## What happens when an endpoint is disconnected

When an endpoint disconnects because its device went away, every application's connection to it is closed, and so is the service's connection to the device. The message queues between processes are shut down. Any messages still waiting in them, in the scheduler, or anywhere else in the service are lost.

If you turned on `AutoReconnect` when you created the connection, the API watches for the endpoint in the background, and reconnects when the endpoint comes back, as long as its id hasn't changed.

## Samples

Use the watcher instead of checking the device count over and over, which is what WinMM made everyone do. Handle `Updated` for as long as the watcher runs, not just for a while after it starts. A MIDI 2.0 endpoint answers discovery after it first appears, and its information is never guaranteed to be final.

* [C++/WinRT watch-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-endpoints)
* [C# watch-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/watch-endpoints)
