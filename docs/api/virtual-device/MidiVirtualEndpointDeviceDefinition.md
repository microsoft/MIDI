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
| `TransportSuppliedDescription` | The description for the endpoint. Optional. |

## Functions

| Function | Description |
| --------------- | ----------- |
| `MidiVirtualEndpointDeviceDefinition()` | Construct a new device definition |


## Example

```cs
var deviceDefinition = new Windows.Devices.Midi2.MidiVirtualEndpointDeviceDefinition();

// Define the first function block
deviceDefinition.FunctionBlocks.Add(new Windows.Devices.Midi2.MidiFunctionBlock()
{
    Number = 0,
    IsActive = true,
    Name = "Pads Output",
    UIHint = midi2.MidiFunctionBlockUIHint.Sender,
    FirstGroupIndex = 0,
    GroupCount = 1,
    Direction = midi2.MidiFunctionBlockDirection.Bidirectional,
    Midi10Connection = midi2.MidiFunctionBlockMidi10.Not10,
    MaxSystemExclusive8Streams = 0,
    MidiCIMessageVersionFormat = 0
});

// Define the section function block
deviceDefinition.FunctionBlocks.Add(new Windows.Devices.Midi2.MidiFunctionBlock()
{
    Number = 1,
    IsActive = true,
    Name = "Controller",
    UIHint = midi2.MidiFunctionBlockUIHint.Sender,
    FirstGroupIndex = 1,
    GroupCount = 1,
    Direction = midi2.MidiFunctionBlockDirection.Bidirectional,
    Midi10Connection = midi2.MidiFunctionBlockMidi10.Not10,
    MaxSystemExclusive8Streams = 0,
    MidiCIMessageVersionFormat = 0
});

deviceDefinition.AreFunctionBlocksStatic = true;
deviceDefinition.EndpointName = "Pad Controller App";
deviceDefinition.EndpointProductInstanceId = "PMB_APP2_3263827";
deviceDefinition.SupportsMidi2ProtocolMessages = true;
deviceDefinition.SupportsMidi1ProtocolMessages = true;
deviceDefinition.SupportsReceivingJRTimestamps = false;
deviceDefinition.SupportsSendingJRTimestamps = false;

// ...

// Create the device and return the connection to that device. Once you
// call Open() on the connection, the client-visible endpoint will be
// enumerated and available for use.
_connection = _session.CreateVirtualDeviceAndConnection(deviceDefinition);
```

[Full Example](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/app-to-app-midi-cs)

## IDL

[MidiVirtualEndpointDeviceDefinition IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiVirtualEndpointDeviceDefinition.idl)
