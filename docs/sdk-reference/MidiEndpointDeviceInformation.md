---
layout: sdk_reference_page
title: MidiEndpointDeviceInformation
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Class containing all the information and metadata about an endpoint you can connect to
---

This class is a specialized equivalent of the `DeviceInformation` WinRT class. It handles requesting all of the additional properties necessary for MIDI devices, and also goes a step further to retrieve parent device information so that applications can display the endpoints and parent devices in context.

We've heard from developers that we did not provide sufficient information about devices in the past, so we created this class and the associated properties to remedy that. We also heard that Async calls were a non-starter for most DAW applications, so everything in this class is synchronous. Finally, because we don't want apps to have to rely upon static port names like they have had to with the WinMM API, there are plenty of properties available here which can be used to identify a UMP endpoint.

> # Note 
> Rather than the `FindAll` or `CreateFrom...` methods, the `MidiEndpointDeviceWatcher` is a better way to retrieve a list of endpoints because you can then keep the watcher open in a background thread, and be notified of property changes, device add/remove, etc. You can find example code for using the `MidiEndpointDeviceWatcher` in the `samples` [folder in the MIDI repo on GitHub](https://github.com/microsoft/MIDI/tree/main/samples).

## Choosing which endpoints to display to your users

When displaying endpoint devices to users, you'll typically want to stick to the defaults: `StandardNativeUniversalMidiPacketFormat | StandardNativeMidi1ByteFormat` which is helpfully combined into `AllStandardEndpoints` value of the `MidiEndpointDeviceInformationFilters` type. You do not want to ever show the Diagnostic Ping in any typical application, and you are unlikely to need to show the system-wide Diagnostic Loopback singletons unless you are specifically offering a diagnostic capability. Finally, you don't want to show the Virtual Device Responder endpoints because those should be reserved only for the "device" application in app-to-app MIDI. 

## In-protocol discovered information

When a device is first enumerated by the MIDI Service, if it is a UMP-native device, we will attempt endpoint discovery and protocol negotiation. During that, we request all endpoint information and all function block information. The received data is then cached in the device properties so that applications do not need to perform this process themselves. This process completes either when a short timeout is hit, or all requested information has been received. Only at that point are MIDI 1.0 API ports created for the MIDI 2.0 device.

For more information about the Endpoint Discovery and Protocol Negotiation aspect of MIDI 2.0, please [see the MIDI 2.0 UMP specification at the MIDI Association web site](https://midi.org/specs).

## Properties

| Property | Source | Description |
| --------------- | ------ | ----------- |
| `EndpointDeviceId` | Windows | The endpoint device interface id. This is sometimes called "the SWD" in short-hand because it's the string that uniquely identifies the software device interface that represents the endpoint. |
| `Name` | Various | This is the name which should be displayed in any application. It calculates the correct name based on the hierarchy of possible names, including a user-specified name. Always respect the user's choice here. The name could be changed at any time and should not be relied upon to be constant from session to session, or even within a single session. |
| `ContainerId` | Windows | The [device container GUID](https://learn.microsoft.com/windows-hardware/drivers/install/container-ids). |
| `DeviceInstanceId` | Windows | The [device instance id](https://learn.microsoft.com/windows-hardware/drivers/install/device-instance-ids) of the endpoint. | 
| `EndpointPurpose` | Windows | The purpose of the endpoint. This is used primarily for filtering. |
| `DeclaredEndpointInfoLastUpdateTime` | Discovery | The time of the last update for endpoint information discovered in-protocol |
| `DeclaredDeviceIdentityLastUpdateTime` | Discovery | The time of the last update for device identity information discovered in-protocol |
| `DeclaredStreamConfigurationLastUpdateTime` | Protocol Negotiation | The time of the last update from protocol negotiation |
| `DeclaredFunctionBlocksLastUpdateTime` | Discovery | The time of the last update of function blocks |
| `Properties` | Returns the raw device properties for this endpoint. The property values and their ids are not something an application should rely upon -- they are an implementation detaul subject to change, and are not part of the contract with apps. Instead, all of the interesting/useful properties have been broken out in other ways with strong types. |

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
| `FindAllAssociatedMidi1PortsForThisEndpoint (portFlow, useCachedPortInformationIfAvailable)` | Finds all the MIDI 1.0 API ports for this device in the specified direction, and returns those as a read-only list of `MidiEndpointAssociatedPortDeviceInformation` objects. Unless using the internal cache, this can be an expensive call time-wise because of how it must iterate through MIDI 1 endpoints in the system. By default, this will refresh the internal cache. If calling multiple times in a row when it is unlikely that devices have been added or removed, use the cache for the lookups to save time by specifying `true` for `useCachedPortInformationIfAvailable`. If the cache has not yet been filled, it will be filled before it is used.|
| `FindAssociatedMidi1PortForGroupForThisEndpoint (portFlow, useCachedPortInformationIfAvailable)` | Finds the MIDI 1.0 API port for this device and MIDI Group in the specified direction and returns that as a `MidiEndpointAssociatedPortDeviceInformation` object, or nullptr if not found. Unless using the internal cache, this can be an expensive call time-wise because of how it must iterate through MIDI 1 endpoints in the system. By default, this will refresh the internal cache. If calling multiple times in a row when it is unlikely that devices have been added or removed, use the cache for the lookups to save time by specifying `true` for `useCachedPortInformationIfAvailable`. If the cache has not yet been filled, it will be filled before it is used.|
| `GetNameTable()` | Returns all the candidate names for MIDI 1.0 API ports created from this UMP endpoint. This is of limited use to applications and is primarily used by the Settings app to enable changing the name of future-created ports. |

## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| `CreateFromId(id)` | Creates a new `MidiEndpointDeviceInformation` object from the specified id |
| `CreateFromAssociatedMidi1PortDeviceId (deviceId)` | Finds the Windows MIDI Services UMP Endpoint associated with the given WinRT MIDI 1.0-compatible device id. This call takes two steps: it must first get the port DeviceInformation object, then check a property on it which points to the parent UMP Endpoint, and then create a `MidiEndpointDeviceInformation` object based on that id. |
| `CreateFromAssociatedMidi1PortNumber (portNumber, portFlow)` | Returns the parent Windows MIDI Services UMP Endpoint for the given WinMM port number. This is a more expensive call to make because it must loop through all MIDI 1.0 ports for the specified direction and find the one with the matching WinMM port number. |
| `FindAll()` | Searches for all endpoint devices and returns a list in the default sort order |
| `FindAll(sortOrder)` | Searches for all endpoint devices and returns a list in the specified sort order |
| `FindAll(sortOrder, endpointFilter)` | Searches for all endpoint devices which match the filter, and returns a list in the specified sort order. |
| `DeviceMatchesFilter(deviceInformation, endpointFilter)` | A helper function to compare a device against the filter. |
| `GetAdditionalPropertiesList()` | This returns the list of properties which must be requested during enumeration. Typically not needed for applications, as the watcher calls this function |
| `FindAllForAssociatedMidi1PortName (portName, portFlow)` | Returns the parent UMP Endpoints for a given MIDI 1 port name. Names are not guaranteed to be unique, but in most cases, this will return a single parent endpoint. |
| `FindAllEndpointDeviceIdsForAssociatedMidi1PortName (portName, portFlow)` | Returns all the Endpoint Device Ids for the parent UMP Endpoints associated with MIDI 1 ports with the specified name. In most cases, this will be a single parent UMP endpoint, but port names are not guaranteed to be unique. |
| `FindEndpointDeviceIdForAssociatedMidi1PortNumber (portNumber, portFlow)` | Returns the Endpoint Device Id for the parent UMP Endpoints associated with MIDI 1 ports with the specified WinMM number. This is a costly lookup as we must loop through MIDI 1.0 ports for the specified direction and inspect a property for each one until we find a match. |
