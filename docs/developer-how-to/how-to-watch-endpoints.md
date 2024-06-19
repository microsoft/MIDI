---
layout: page
title: Enumerate UMP Endpoints (with add/remove/change detection)
parent: Developer How-to
has_children: false
---

# Enumerate UMP Endpoints (with add/remove/change detection)

In WinRT, a device watcher is a type class which raises events when specific devices are added, removed, or have had property changes. The concept of a device watcher is central to working with all types of devices in Windows. The Windows SDK ships with a general `Windows::Devices::Enumeration::DeviceWatcher` class which can be used for any type of device query.

In Windows MIDI Services, we've provided a specialized version of the device watcher called the `Microsoft::Windows::Devices::Midi2::MidiEndpointDeviceWatcher`. This class takes the research and guesswork out of watching a device, and supports the additional information we capture in custom device properties.

> Using a device watcher is a best practice for most MIDI applications. However, if you want to retrieve a one-time snapshot of active devices, see the [How to Enumerate Endpoints](how-to-enumerate-endpoints.md) document.

## Events

To use the `MidiEndpointDeviceWatcher`, first wire up handlers for the `Added`, `Removed`, and `Updated` events. Optionally, you may wire up handlers for the `EnumerationCompleted` event to be notified when initial enumeration has finished, and the `Stopped` event to know when the watcher has been stopped by a call to the `Stop` method.

Once the event handlers have been wired up, create the watcher using the static `Create` function.

```cpp
auto watcher = MidiEndpointDeviceWatcher::Create();
```

If you wish to use a filter list that differs from the default (the default is appropriate for most applications, as it filters out diagnostics and other endpoints not typically shown to end users) you may use the overloaded Create function. For example, to show only native UMP endpoints, not translated MIDI 1.0 devices, you would do this:

```cpp
auto watcher = MidiEndpointDeviceWatcher::Create(MidiEndpointDeviceInformationFilters::IncludeClientUmpFormatNative);
```

And, conversely, to show only up-converted MIDI 1.0 byte format endpoints:

```cpp
auto watcher = MidiEndpointDeviceWatcher::Create(MidiEndpointDeviceInformationFilters::IncludeClientByteFormatNative);
```

The default is to include both, which is also represented by `MidiEndpointDeviceInformationFilters::AllTypicalEndpoints`.

## Accessing the list

An application may track added / updated / removed devices in its own container, by handling the appropriate events, or it may use the `EnumeratedEndpointDevices` map. 

That this map is dynamic, and changes when devices are added or removed. For that reason, we recommend not caching copies of the objects in there, but instead accessing them by the `EndpointDeviceId` (the map key) each time. This will help ensure you always have the latest properties, and also that you are not holding references to invalidated (removed) objects.

## Samples

* [C++/WinRT Endpoint Device Watcher Sample](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-endpoints)


