---
layout: sdk_reference_page
title: MidiVirtualDeviceManager
namespace: Windows.Devices.Midi2.Transports.Virtual
type: runtimeclass
description: The interface to the service for creating a virtual device
---

Apps use this class to create new virtual devices, for app-to-app MIDI.

## Static Properties

| Name | Description |
| --------------- | ----------- |
| `IsTransportAvailable` | True if this transport is installed and turned on in the service |
| `TransportId` | The GUID of the virtual device transport |

## Static Methods

| Name | Description |
| --------------- | ----------- |
| `CreateVirtualDevice(creationConfig)` | Creates a new virtual device with this configuration. Returns the `MidiVirtualDevice` |
| `GetAssociatedClientEndpointDeviceId(associationId)` | Returns the endpoint device id of the client endpoint for this association id |

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/simple-app-to-app-midi)
* [C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
