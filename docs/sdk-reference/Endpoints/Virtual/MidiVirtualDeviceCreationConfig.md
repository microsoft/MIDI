---
layout: sdk_reference_page
title: MidiVirtualDeviceCreationConfig
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Virtual
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Information supplied when creating a new virtual device
---

The `MidiVirtualDeviceCreationConfig` class specifies, in an easy to use format, the responses for endpoint discovery as well as the properties to use when constructing the device endpoint.

## Properties

| Property | Description |
| --------------- | ----------- |
| `AssociationId` | Returns the id which links the device and client endpoints |
| `Name` | The transport-supplied name to use for this device |
| `Description` | The transport-supplied description to use for this device |
| `Manufacturer` | The transport-supplied manufacturer name to use for this device |
| `DeclaredDeviceIdentity` | The `MidiDeclaredDeviceIdentity` to use for responding to MIDI 2.0 endpoint discovery |
| `DeclaredEndpointInfo` | The `MidiDeclaredEndpointInfo` to use for responding to MIDI 2.0 endpoint discovery |
| `UserSuppliedInfo` | Any user-supplied information for this endpoint  |
| `FunctionBlocks` | The set of function blocks to be declared for this endpoint |
| `CreateOnlyUmpEndpoints` | True if only UMP endpoints should be created for this endpoint (no WinMM or WinRT MIDI 1.0 Ports) |

## Methods

| Name | Description |
| --------------- | ----------- |
| `MidiVirtualDeviceCreationConfig(name, description, manufacturer, declaredEndpointInfo)` | Create a virtual device with the specified information|
| `MidiVirtualDeviceCreationConfig(name, description, manufacturer, declaredEndpointInfo, declaredDeviceIdentity)` | Create a virtual device with the specified information |
| `MidiVirtualDeviceCreationConfig(name, description, manufacturer, declaredEndpointInfo, declaredDeviceIdentity, userSuppliedInfo)` | Create a virtual device with the specified information |

## Examples

[C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/simple-app-to-app-midi/main.cpp)
[C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
