---
layout: kb
title: Integrating MIDI 1.0 (like WinMM and WinRT MIDI 1.0) and MIDI 2.0 APIs
audience: developers
description: This document explains how to associate MIDI 1.0 ports and Windows MIDI Services endpoints
---

# Background

Before reviewing this, [please see "Mapping MIDI 1.0 Port Concepts" here](/mapping-midi1-port-concepts.md)

Although the modern UMP-based MIDI 1.0 and MIDI 2.0 Windows.Devices.Midi2 API works with both MIDI 1.0 and MIDI 2.0 devices, there are times when you may want to partially transition to the new API while retaining existing legacy MIDI 1 API functionality, or you may need to map between artifacts from the two APIs.

## The Legacy enumeration namespace

`Windows.Devices.Midi2.Enumeration` includes a `Legacy` sub-namespace specifically focused on integration with older APIs. This includes a class for WinMM/WinRT MIDI 1.0 port device information, as well as a Device Watcher implementation just for MIDI 1.0 ports.

Documentation for this namespace is here TODO_URL

# Scenarios using both legacy and the modern API.

The WinMM MIDI 1.0 API contains limited functionality for identifying devices. Most apps ended up using Port names for identifiers, although some used the `DRV_QUERYDEVICEINTERFACE` message to help find parent hardware devices. Using port names is brittle, especially now that users can rename ports, but I can't argue against 30+ years of application code. :)

Although the intent is to move everyone to the new API, the new MIDI API includes functionality that makes it reasonable to partially update your app to be more robust, without requiring you completely switch over to it completely.

## Subscribe to port update/remove notifications with the Watcher

Today, many apps have a loop which repeatedly calls `MidiIn/OutGetNumDevs` and looks for changes. When done on the UI thread, this isn't a great idea, but it works. (Another option would have been to use the CM_NOTIFY functionality to look for devices with the right interface classes -- an example is this may be found in the wdmaud2.drv project in [MidiSrvPorts.cpp](https://github.com/microsoft/MIDI/blob/main/src/api/Client/WinMM/MidiSrvPorts.cpp)) 

Another approach is to set up a Device Watcher. This is a WinRT concept which raises events when devices are added/updated or removed. Because we store many custom properties in our created MIDI devices, the API includes specialized device watchers:
- `Windows.Devices.Midi2.Enumeration.MidiEndpointDeviceWatcher` for the Endpoints created for use with the new API
- `Windows.Devices.Midi2.Enumeration.Legacy.MidiLegacyPortDeviceWatcher` for MIDI 1.0 ports

When you create a Device Watcher, and then start it, it will begin enumerating all the matching entities discoverd. After initial enumeration is complete, you will continue to receive events when they are added, removed, or updated. 

Using the MidiLegacyPortDeviceWatcher, the updates include when the name is updated, and also when the WinMM Port Number is changed. The former can happen when a user changes the naming approach or provides a custom name for a port. The latter can happen when any ports with a lower number are removed. Because apps expect port numbering to be contiguous, we must compact the list whenever a device is removed.

This can be used alongside existing WinMM API calls for device discovery, or instead of them. The port numbers exposed in the `MidiLegacyPortDeviceInformation` type can be used in any WinMM API which requires a WinMM Port Number/Index.

Using the Watcher is an effective way to identify port presence. It can take a few seconds after a device is plugged in (or a virtual device created) before the MIDI 1.0 ports come online. The Watcher will only report new device presence once it is available for use.

Underneath the specialized watcher is a regular WinRT Device watcher with a specialized query and proprties list. The documentation for WinRT Device Watchers applies to these specialized watchers.

## Maintain an up-to-date list of all MIDI 1.0 ports

The `MidiLegacyPortDeviceWatcher` maintains a list of all MIDI 1.0 ports, that is kept in sync with added/removed/updated notifications.

## Find all MIDI 1.0 ports

If you don't need the updates and efficiency provided by a Device Watcher, you can statically enumerate all the MIDI 1.0 ports, or just the sources (inputs) or destinations (outputs) using the static methods on the `Windows.Devices.Midi2.Enumeration.Legacy.MidiLegacyPortDeviceInformation` type.

```cpp
static IVectorView<MidiLegacyPortDeviceInformation> FindAll()
static IVectorView<MidiLegacyPortDeviceInformation> FindAll(Windows.Devices.Midi2.Enumeration.Midi1PortFlow flow)
```

## Find all MIDI 1.0 ports associated with a specific Endpoint

The same `Windows.Devices.Midi2.Enumeration.Legacy.MidiLegacyPortDeviceInformation` type includes static functions to get all of the MIDI 1.0 ports, or just the sources (inputs) or destinations (outputs), for a given UMP Endpoint. Simply use the `EndpointDeviceId` from the endpoint as the parameter.

```cpp
static IVectorView<MidiLegacyPortDeviceInformation> FindAllForEndpoint(String endpointDeviceId)
static IVectorView<MidiLegacyPortDeviceInformation> FindAllForEndpoint(String endpointDeviceId, Windows.Devices.Midi2.Enumeration.Midi1PortFlow flow)
```

## Find all MIDI 1.0 ports associated with a specific name

Although we want to help move away from using port names as a persistent identifer, as stated in the intro, we recognize this is not always possible. Therefore the `MidiLegacyPortDeviceInformation` type provides a function which looks up MIDI 1.0 ports by name.

```cpp
static IVectorView<MidiLegacyPortDeviceInformation> FindAllForName(String portName)
```

Note that this is using only the current name of the port. We do not have lookups for the original name, or the different styles of names from the name table because those fields are custom and cannot be indexed or included in an AQS query. Therefore, there's no performance benefit to providing a function for those. Instead, you can iterate through the UMP Endpoint information objects, inspect their properties for the name table, and find the information that way.

If we are able to get those properties indexed in the future, we will update the types here to make use of them.

## Find the MIDI 1.0 port associated with an endpoint, by Group Index and Direction

For this, we recommend using one of the static `MidiLegacyPortDeviceInformation` methods to find the MIDI 1 ports for an endpoint (or use the list of all found ports that the Device Watcher maintains) and then look at the `Group` and `PortFlow` properties.

## Find the MIDI 1.0 port by Port Number and Direction

For this, we recommend using one of the static `MidiLegacyPortDeviceInformation` methods to find all MIDI 1.0 ports (or use the list of all found ports that the Device Watcher maintains) and then look at the `WinMMPortNumber` and `PortFlow` properties.

## Getting to the parent UMP endpoint for a MIDI 1.0 Port

All MIDI 1.0 ports enumerated by the new MIDI Service have the UMP Endpoint as their parent. 

To get to the parent endpoint from a port, use the `EndpointDeviceId` property of the port as the parameter into `MidiEndpointDeviceInformation::CreateFromId()` or similar functions. From there, you will have access to a rich set of properties attached to the MIDI Endpoint to help your app make decisions about how to use the device.


## Getting VID/PID and other identifiers for MIDI 1.0 ports on a device

For any MIDI device using the new USBMIDI2-ACX class driver, the MIDI Service requests VID/PID/Serial when enumerating the device. This is part of the new device driver interface and was designed to make it easier for apps to identify devices (for example, to know if the device is an appropriate one for an editor app).

For devices not using the new driver, we attempt to parse the VID/PID/Serial from the Device Instance Id. In the future, we may add the VID/PID/Serial request to our MIDI 1.0 driver, but that is not in place at the time of this writing. The Windows Property System doesn't current expose VID/PID/Serial as a specific set of properties.

Once you get the parent UMP endpoint as described above, call `GetTransportSuppliedInformation` to get details from the transport plugin in the service. For the MIDI 1.0 and MIDI 2.0 USB driver transports (KSA and KS, respectively) this will include any available USB VID/PID/Serial information as well as the original transport-supplied name for the endpoint (without any user customization applied).

Although technically the VID/PID/Serial is information about the parent, we provide it here because we are able to get it directly from the driver. The MIDI Service is not the owner of the original device, so it cannot add new properties to that device object.

## Getting information about the actual hardware device

Each UMP Endpoint has a parent. In the case of USB devices, that will be a physical hardware device.

A call to `MidiEndpointDeviceInformation.GetParentDeviceInformation()` on an existing endpoint will return all the parent device information we believe to be relevant for use with MIDI. This also includes any driver information we were able to retrieve about the device. Note that outside of USB, most transports do not require kernel drivers, so much of this information will be empty.

## Getting the original MIDI 1.0 port names for a user-renamed port




