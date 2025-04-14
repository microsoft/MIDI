---
layout: api_page
title: MidiEndpointDeviceInformation
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointDeviceInformation

This class is a specialized equivalent of the `DeviceInformation` WinRT class. It handles requesting all of the additional properties necessary for MIDI devices, and also goes a step further to retrieve parent device information so that applications can display the endpoints and parent devices in context.

We've heard from developers that we did not provide sufficient information about devices in the past, so we created this class and the associated properties to remedy that. We also heard that Async calls were a non-starter for most DAW applications, so everything in this class is synchronous.

> Note: the MidiEndpointDeviceWatcher is a better way to retrieve devices because you can then keep the watcher open in a background thread, and be notified of property changes, device add/remove, etc.

When displaying endpoint devices to users, you'll typically want to stick to the defaults: `IncludeClientUmpFormatNative | IncludeClientByteFormatNative`. You do not want to show the Diagnostic Ping ever, and you typically will not want to show the system-wide Diagnostic Loopback singletons. Finally, you don't want to show the Virtual Device Responder endpoints because those should be reserved only for the "device" application in app-to-app MIDI. 

## In-protocol discovered information

When a device is first enumerated by the MIDI Service, if it is a UMP-native device, we will attempt endpoint discover and protocol negotiation. During that, we request all endpoint information and all function block information. The received data is then cached in the device properties so that applications do not need to perform this process themselves.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Implements

`IStringable`

## Properties

| Property | Source | Description |
| --------------- | ------ | ----------- |
| `EndpointDeviceId` | Windows | The endpoint device interface id. This is sometimes called "the SWD" in short-hand because it's the string that uniquely identifies the software device interface that represents the endpoint. |
| `Name` | Various | This is the name which should be displayed in any application. It calculates the correct name based on the hierarchy of possible names, including a user-specified name. Always respect the user's choice here. |
| `ContainerId` | Windows | The device container |
| `DeviceInstanceId` | Windows | The device instance id without the interface information | 
| `EndpointPurpose` | Windows | The purpose of the endpoint. This is used primarily for filtering. |
| `DeclaredEndpointInfoLastUpdateTime` | Discovery | The time of the last update for endpoint information discovered in-protocol |
| `DeclaredDeviceIdentityLastUpdateTime` | Discovery | The time of the last update for device identity information discovered in-protocol |
| `DeclaredStreamConfigurationLastUpdateTime` | Protocol Negotiation | The time of the last update from protocol negotiation |
| `DeclaredFunctionBlocksLastUpdateTime` | Discovery | The time of the last update of function blocks |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `EndpointInterfaceClass` | The class GUID which appears at the end of the Endpoint Ids |

## Functions

| Function | Description |
| --------------- | ----------- |
| `GetDeclaredEndpointInfo()` | Returns a `MidiDeclaredEndpointInfo` structure with the currently stored endpoint discovery information |
| `GetDeclaredDeviceIdentity()` | Returns a `MidiDeclaredDeviceIdentity` structure with the currently stored device identity information |
| `GetDeclaredStreamConfiguration()` | Returns a `MidiDeclaredStreamConfiguration` structure with the currently stored stream configuration |
| `GetDeclaredFunctionBlocks()` | Returns a snapshot of the currently stored function blocks |
| `GetGroupTerminalBlocks()` | Returns the currently stored group terminal blocks (USB devices only) |
| `GetUserSuppliedInfo()` | Returns a `MidiEndpointUserSuppliedInfo` structure currently stored user-supplied information |
| `GetTransportSuppliedInfo()` | Returns a `MidiEndpointTransportSuppliedInfo` with the currently stored transport-supplied information |
| `GetParentDeviceInformation()` | Finds and then retrieves the parent `DeviceInformation` type with appropriate properties. |
| `GetContainerInformation()` | Gets the device container information and returns its `DeviceInformation` with appropriate properties |
| `FindAllAssociatedMidi1PortsForThisEndpoint` | Finds all the MIDI 1.0 API ports for this device in the specified direction, and returns those as a read-only list of `MidiEndpointAssociatedPortDeviceInformation` objects. Unless using the internal cache, this can be an expensive call time-wise because of how it must iterate through MIDI 1 endpoints in the system. By default, this will refresh the internal cache. If calling multiple times in a row when it is unlikely that devices have been added or removed, use the cache for the lookups to save time by specifying `true` for `useCachedPortInformationIfAvailable`. If the cache has not yet been filled, it will be filled before it is used.|
| `FindAssociatedMidi1PortForGroupForThisEndpoint` | Finds the MIDI 1.0 API port for this device and MIDI Group in the specified direction and returns that as a `MidiEndpointAssociatedPortDeviceInformation` object, or nullptr if not found. Unless using the internal cache, this can be an expensive call time-wise because of how it must iterate through MIDI 1 endpoints in the system. By default, this will refresh the internal cache. If calling multiple times in a row when it is unlikely that devices have been added or removed, use the cache for the lookups to save time by specifying `true` for `useCachedPortInformationIfAvailable`. If the cache has not yet been filled, it will be filled before it is used.|

## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| `CreateFromId(id)` | Creates a new `MidiEndpointDeviceInformation` object from the specified id |
| `CreateFromAssociatedMidi1PortDeviceId` | Finds the Windows MIDI Services UMP Endpoint associated with the given WinRT MIDI 1.0-compatible device id. This call takes two steps: it must first get the port DeviceInformation object, then check a property on it which points to the parent UMP Endpoint, and then create a `MidiEndpointDeviceInformation` object based on that id. |
| `CreateFromAssociatedMidi1PortNumber` | Returns the parent Windows MIDI Services UMP Endpoint for the given WinMM port number. This is a more expensive call to make because it must loop through all MIDI 1.0 ports for the specified direction and find the one with the matching WinMM port number. |
| `FindAll()` | Searches for all endpoint devices and returns a list in the default sort order |
| `FindAll(sortOrder)` | Searches for all endpoint devices and returns a list in the specified sort order |
| `FindAll(sortOrder, endpointFilter)` | Searches for all endpoint devices which match the filter, and returns a list in the specified sort order. |
| `DeviceMatchesFilter(deviceInformation, endpointFilter)` | A helper function to compare a device against the filter. |
| `GetAdditionalPropertiesList()` | This returns the list of properties which must be requested during enumeration. Typically not needed for applications, as the watcher calls this function |
| `FindAllForAssociatedMidi1PortName` | Returns the parent UMP Endpoints for a given MIDI 1 port name. Names are not guaranteed to be unique, but in most cases, this will return a single parent endpoint. |
| `FindAllEndpointDeviceIdsForAssociatedMidi1PortName` | Returns all the Endpoint Device Ids for the parent UMP Endpoints associated with MIDI 1 ports with the specified name. In most cases, this will be a single parent UMP endpoint, but port names are not guaranteed to be unique. |
| `FindEndpointDeviceIdForAssociatedMidi1PortNUmber` | Returns the Endpoint Device Id for the parent UMP Endpoints associated with MIDI 1 ports with the specified WinMM number. This is a costly lookup as we must loop through MIDI 1.0 ports for the specified direction and inspect a property for each one until we find a match. |


## IDL

[MidiEndpointDeviceInformation IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiEndpointDeviceInformation.idl)

