---
layout: api_page
title: MidiVirtualEndpointDeviceDefinition
parent: Virtual Devices
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiVirtualEndpointDeviceDefinition

The `MidiVirtualEndpointDeviceDefinition` class specifies, in an easy to use format, the responses for discovery and protocol negotiation, as well as the properties to use when constructing the device endpoint.

## Properties

| Property | Description |
| --------------- | ----------- |
| `EndpointName` | Name of the endpoint used for both the device enumeration and for responding to the endpoint name request UMP message |
| `EndpointProductInstanceId` | The unique identifier for this device. This value is used when creating the device Id, and is also used as the response for endpoint discovery when the id is requested. In general, this value should be kept to less than 32 characters and not include any special characters or symbols |
| `SupportsMidi1ProtocolMessages` | For endpoint discovery. True if this endpoint supports MIDI 1.0 protocol messages over UMP |
| `SupportsMidi2ProtocolMessages` | For endpoint discovery. True if this endpoint supports MIDI 2.0 protocol messages over UMP |
| `SupportsReceivingJRTimestamps` | For endpoint discovery. True if this endpoint supports recieving JR timestamps (typically, you'll want to set this to false) |
| `SupportsSendingJRTimestamps` | For endpoint discovery. True if this endpoint supports sending JR timestamps (typically, you'll want to set this to false) |
| `DeviceManufacturerSystemExclusiveId` | MIDI 2.0 UMP Device Identity value|
| `DeviceFamilyLsb` | MIDI 2.0 UMP Device Identity value |
| `DeviceFamilyMsb` | MIDI 2.0 UMP Device Identity value |
| `DeviceFamilyModelLsb` | MIDI 2.0 UMP Device Identity value |
| `DeviceFamilyModelMsb` | MIDI 2.0 UMP Device Identity value |
| `SoftwareRevisionLevel` | MIDI 2.0 UMP Device Identity value |
| `AreFunctionBlocksStatic` | True if the function blocks will not change in any way |
| `FunctionBlocks` | list of function blocks for this device |

## Functions

| Function | Description |
| --------------- | ----------- |
| `MidiVirtualEndpointDeviceDefinition()` | Construct a new device definition |


## IDL

[MidiVirtualEndpointDeviceDefinition IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiVirtualEndpointDeviceDefinition.idl)
