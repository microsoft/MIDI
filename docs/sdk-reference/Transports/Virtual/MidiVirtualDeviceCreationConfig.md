---
layout: sdk_reference_page
title: MidiVirtualDeviceCreationConfig
namespace: Windows.Devices.Midi2.Transports.Virtual
type: runtimeclass
description: Information supplied when creating a new virtual device
---

`MidiVirtualDeviceCreationConfig` holds the answers to endpoint discovery, and the properties to use when the device endpoint is created.

## Properties

| Property | Description |
| --------------- | ----------- |
| `AssociationId` | The id that links the device endpoint and the client endpoint |
| `Name` | The name to use for this device. It becomes the transport-supplied name |
| `Description` | The description to use for this device. It becomes the transport-supplied description |
| `Manufacturer` | The manufacturer name to use for this device. It becomes the transport-supplied manufacturer name |
| `DeclaredDeviceIdentity` | The `MidiDeclaredDeviceIdentity` to use when answering MIDI 2.0 endpoint discovery |
| `DeclaredEndpointInfo` | The `MidiDeclaredEndpointInfo` to use when answering MIDI 2.0 endpoint discovery |
| `UserSuppliedInfo` | Any user-supplied information for this endpoint |
| `FunctionBlocks` | The function blocks to declare for this endpoint |
| `CreateOnlyUmpEndpoints` | True to create only UMP endpoints, with no WinMM or WinRT MIDI 1.0 ports |

## Constructors

| Constructor | Description |
| --------------- | ----------- |
| `MidiVirtualDeviceCreationConfig(name, description, manufacturer, declaredEndpointInfo)` | Creates a configuration with this information |
| `MidiVirtualDeviceCreationConfig(name, description, manufacturer, declaredEndpointInfo, declaredDeviceIdentity)` | Creates a configuration with this information and device identity |
| `MidiVirtualDeviceCreationConfig(name, description, manufacturer, declaredEndpointInfo, declaredDeviceIdentity, userSuppliedInfo)` | Creates a configuration with all of this information |

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/simple-app-to-app-midi)
* [C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
