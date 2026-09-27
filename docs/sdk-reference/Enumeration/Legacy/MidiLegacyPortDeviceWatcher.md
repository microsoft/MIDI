---
layout: sdk_reference_page
title: MidiLegacyPortDeviceWatcher
namespace: Windows.Devices.Midi2.Enumeration.Legacy
type: runtimeclass
description: Watcher for MIDI 1.0 legacy port devices
---

This works like `MidiEndpointDeviceWatcher`, but for the MIDI 1.0 ports Windows MIDI Services creates.

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `Create()` | Creates a watcher for every MIDI 1.0 port |
| `CreateForFlow(flow)` | Creates a watcher for the MIDI 1.0 ports that go in this `Midi1PortFlow` direction |

## Methods

| Method | Description |
| ------ | ----------- |
| `Start()` | Starts finding ports. Attach your event handlers before you call this. |
| `Stop()` | Stops finding ports. |
| `GetEnumeratedPortsForFlow(flow)` | The ports found so far that go in one direction. |
| `GetEnumeratedPortForNumber(portNumber, flow)` | The port with this WinMM port number and direction, or null. |
| `GetEnumeratedPortsForParent(parentDeviceInstanceId)` | The ports that belong to one parent device. Another version also takes a `flow`. |
| `GetEnumeratedPortsForAssociatedEndpoint(endpointDeviceId)` | The ports that belong to one UMP endpoint. Another version also takes a `flow`. |
| `GetEnumeratedPortsForAssociatedEndpointAndGroup(endpointDeviceId, group)` | The ports for one group of one UMP endpoint. Another version also takes a `flow`. |
| `GetEnumeratedPortsForName(portName)` | The ports with this name. Names aren't unique, so this returns a collection. Another version also takes a `flow`. |
| `GetEnumeratedPortsForDriverDeviceInterfaceId(driverDeviceInterfaceId)` | The ports that come from one driver device interface. Another version also takes a `flow`. |

These all work on the ports the watcher has already found, as they are right now. None of them asks Windows again, so a port that appeared since the last event won't be in the result.

## Properties

| Property | Description |
| -------- | ----------- |
| `Status` | The watcher's current `DeviceWatcherStatus` |
| `EnumeratedPorts` | A map of every MIDI 1.0 port found so far. The key is the port's device id |
| `CountSourcePorts` | How many source (input) ports have been found |
| `CountDestinationPorts` | How many destination (output) ports have been found |

## Events

The underlying `DeviceWatcher` raises these events and waits for your handlers, so the same rules and advice apply as for the WinRT `Windows.Devices.Enumeration.DeviceWatcher` type. We also recommend:

- Don't do slow work in an event handler.
- Don't create or remove virtual devices, loopbacks, or network connections in a handler, and don't change properties in a way that would cause more Plug and Play events.
- If you're not sure, pass the data to a worker thread and return from the handler quickly.

| Event | Description |
| ----- | ----------- |
| `Added(source, args)` | Raised when a MIDI 1.0 port is added |
| `Removed(source, args)` | Raised when a MIDI 1.0 port is removed |
| `Updated(source, args)` | Raised when a MIDI 1.0 port's properties change |
| `EnumerationCompleted(source)` | Raised when the first pass of finding ports is done |
| `Stopped(source)` | Raised when the watcher stops |
