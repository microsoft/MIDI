---
layout: sdk_reference_page
title: MidiVirtualDevice
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Virtual
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: IMidiEndpointMessageProcessingPlugin
description: Represents a virtual device in app-to-app MIDI
---

This is the class that a virtual device application uses as its interface to the virtual device it has defined. Use the `MidiVirtualDeviceManager` to construct an instance of this type.

## Properties

| Property | Description |
| --------------- | ----------- |
| `DeviceEndpointDeviceId` | The EndpointDeviceId to be used by the app creating the virtual device |
| `AssociationId` | The id used to associate the client and device endpoints |
| `FunctionBlocks` | Current list of function blocks for this device. |
| `SuppressHandledMessages` | True if the protocol messages handled by this class should be filtered out of the incoming message stream |

## Functions

| Function | Description |
| --------------- | ----------- |
| `UpdateFunctionBlock` | Update the properties of a single function block. The number of actual function blocks cannot change after creation (per the UMP specification) but blocks may be marked as active or inactive. Changes here will result in the MIDI 2.0 function block notification messages being sent out. |
| `UpdateEndpointName` | Update the endpoint name, and send out the appropriate endpoint name notification messages. |

## Events

| Event | Description |
| --------------- | ----------- |
| `StreamConfigRequestReceived(device, args)` | Raised when this device receives a Stream Configuration Request UMP message. |

## Examples

[C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/simple-app-to-app-midi/main.cpp)
[C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
