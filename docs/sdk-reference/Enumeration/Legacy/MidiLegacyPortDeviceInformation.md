---
layout: sdk_reference_page
title: MidiLegacyPortDeviceInformation
namespace: Windows.Devices.Midi2.Enumeration.Legacy
type: runtimeclass
description: Information about a legacy MIDI 1.0 port device
---

Information about one MIDI 1.0 port, either a source or a destination, that Windows MIDI Services created for a UMP endpoint. To hear about changes, use `MidiLegacyPortDeviceWatcher`.

## Properties

| Property | Description |
| -------- | ----------- |
| `PortDeviceId` | The full device id of this MIDI 1.0 port |
| `PortDeviceInstanceId` | The device instance id of this MIDI 1.0 port |
| `Name` | The friendly name of the port |
| `ContainerId` | The container GUID for this device |
| `TransportId` | The GUID of the transport that created the UMP endpoint behind this port |
| `AssociatedEndpointDeviceId` | The full device id of the Windows MIDI Services UMP endpoint this port belongs to |
| `ParentDeviceInstanceId` | The device instance id of the parent device |
| `DriverDeviceInterfaceId` | The driver's device interface id |
| `Group` | The MIDI 2.0 group (`MidiGroup`) this port uses |
| `Flow` | Which way the port goes (`Midi1PortFlow`): source or destination |
| `Number` | The WinMM port number for this port |
| `NativeDataFormat` | The `MidiEndpointNativeDataFormat` of the endpoint this port belongs to |
| `Properties` | The raw device properties |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetParentDeviceInformation()` | Returns the `MidiParentDeviceInformation` for the parent device |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `Midi1SourcePortInterfaceClass` | The interface class GUID for MIDI 1.0 source ports |
| `Midi1DestinationPortInterfaceClass` | The interface class GUID for MIDI 1.0 destination ports |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateFromPortDeviceId(portDeviceId)` | Creates a `MidiLegacyPortDeviceInformation` for the port with this device id |
| `FindAll()` | Returns every MIDI 1.0 port |
| `FindAll(flow)` | Returns every MIDI 1.0 port that goes in this direction |
| `FindAllForAssociatedEndpoint(endpointDeviceId)` | Returns every MIDI 1.0 port that belongs to this UMP endpoint |
| `FindAllForAssociatedEndpoint(endpointDeviceId, flow)` | Returns every MIDI 1.0 port that belongs to this UMP endpoint and goes in this direction |
| `FindAllForName(portName)` | Returns every MIDI 1.0 port with this name |
| `FindAllForContainer(containerId)` | Returns every MIDI 1.0 port in this device container |
| `GetAdditionalPropertiesList()` | Returns the list of extra properties to ask for when you enumerate ports yourself |
