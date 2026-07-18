---
layout: sdk_reference_page
title: MidiLegacyPortDeviceInformation
namespace: Windows.Devices.Midi2.Enumeration.Legacy
type: runtimeclass
description: Information about a legacy MIDI 1.0 port device
---

This class provides information about a MIDI 1.0 port (source or destination) created by Windows MIDI Services for a UMP endpoint. Use `MidiLegacyPortDeviceWatcher` to monitor for changes.

## Properties

| Property | Description |
| -------- | ----------- |
| `PortDeviceId` | The full device id for this MIDI 1.0 port |
| `PortDeviceInstanceId` | The device instance id for this MIDI 1.0 port |
| `Name` | The friendly name of the port |
| `ContainerId` | The container GUID for this device |
| `AssociatedEndpointDeviceId` | The full device id of the parent Windows MIDI Services UMP endpoint |
| `ParentDeviceInstanceId` | The device instance id of the parent device |
| `DriverDeviceInterfaceId` | The driver device interface id |
| `Group` | The `MidiGroup` (MIDI 2.0 group) associated with this port |
| `Flow` | The `Midi1PortFlow` direction (source or destination) |
| `Number` | The WinMM port number for this port |
| `NativeDataFormat` | The `MidiEndpointNativeDataFormat` of the parent endpoint |
| `Properties` | Raw device properties bag |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetParentDeviceInformation()` | Returns the `MidiParentDeviceInformation` for the parent device |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `Midi1SourcePortInterfaceClass` | GUID for the MIDI 1.0 source port interface class |
| `Midi1DestinationPortInterfaceClass` | GUID for the MIDI 1.0 destination port interface class |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateFromPortDeviceId(portDeviceId)` | Creates a `MidiLegacyPortDeviceInformation` from the given port device id |
| `FindAll()` | Returns all MIDI 1.0 ports |
| `FindAll(flow)` | Returns all MIDI 1.0 ports in the specified direction |
| `FindAllForAssociatedEndpoint(endpointDeviceId)` | Returns all MIDI 1.0 ports associated with the given UMP endpoint |
| `FindAllForAssociatedEndpoint(endpointDeviceId, flow)` | Returns all MIDI 1.0 ports associated with the given UMP endpoint in the specified direction |
| `FindAllForName(portName)` | Returns all MIDI 1.0 ports with the specified name |
| `FindAllForContainer(containerId)` | Returns all MIDI 1.0 ports in the specified device container |
| `GetAdditionalPropertiesList()` | Returns the list of additional properties to request during enumeration |
