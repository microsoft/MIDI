---
layout: api_page
title: MidiVirtualDevice
parent: Midi2.Endpoints.Virtual
has_children: false
---

# MidiVirtualDevice

## Properties

| Property | Description |
| --------------- | ----------- |
| `DeviceEndpointDeviceId` | The EndpointDeviceId to be used by the app creating the virtual device |
| `ClientEndpointDeviceId` | The EndpointDeviceId to be used by apps which want to connect to this virtual device |


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

## IDL

[MidiVirtualDevice IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-endpoints-virtual/MidiVirtualDevice.idl)
