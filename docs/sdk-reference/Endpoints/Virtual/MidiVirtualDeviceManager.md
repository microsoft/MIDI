---
layout: sdk_reference_page
title: MidiVirtualDeviceManager
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Virtual
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: The interface to the service for creating a virtual device
---

This is the class used by applications to create and start new virtual (app-to-app MIDI) devices.

## Static Properties

| Name | Description |
| --------------- | ----------- |
| `IsTransportAvailable` | Returns true if this transport is installed and enabled in the service |
| `TransportId` | Returns the GUID which uniquely identifies the Virtual Device transport |

## Static Methods

| Name | Description |
| --------------- | ----------- |
| `CreateVirtualDevice(creationConfig)` | Creates a new virtual device with the specified configuration |

## Examples

[C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/simple-app-to-app-midi/main.cpp)
[C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
