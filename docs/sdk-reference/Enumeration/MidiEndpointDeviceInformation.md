---
layout: sdk_reference_page
title: MidiEndpointDeviceInformation
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Class containing all the information and metadata about an endpoint you can connect to
---

This class is like the WinRT `DeviceInformation` class, built for MIDI. It asks Windows for all the extra properties MIDI devices need. It also gets information about the parent device, so your application can show each endpoint along with the device it belongs to.

Developers told us that, in the past, we didn't give them enough information about devices, so we created this class to fix that. They also told us that async calls don't work for most DAW applications, so everything in this class is synchronous. And because we don't want applications to depend on fixed port names, the way they had to with WinMM, there are plenty of properties here you can use to identify a UMP endpoint.

> **Note:** `MidiEndpointDeviceWatcher` is a better way to get a list of endpoints than the `FindAll` or `CreateFrom...` methods. You can keep the watcher running on a background thread, and it tells you when devices are added or removed, and when their properties change. You'll find example code for `MidiEndpointDeviceWatcher` in the [samples folder in the MIDI repo on GitHub](https://github.com/microsoft/MIDI/tree/main/samples).

## Choosing which endpoints to show

When you show endpoints to people, you'll usually want the defaults: `StandardNativeUniversalMidiPacketFormat | StandardNativeMidi1ByteFormat`. The `MidiEndpointDeviceInformationFilters` type combines those two into `AllStandardEndpoints` for you. Never show the diagnostic ping endpoint in a normal application. You probably don't need to show the two built-in diagnostic loopback endpoints either, unless your application offers diagnostic features. And don't show the virtual device responder endpoints, because only the "device" application in app-to-app MIDI should use those.

## Information the device gives during discovery

When the MIDI service first finds a device that natively uses UMP, it tries endpoint discovery and protocol negotiation. It asks for all the endpoint information and all the function block information. The answers are saved in the device properties, so applications don't have to do this themselves. This finishes when all the requested information has arrived, or after a short timeout. Only then are the MIDI 1.0 ports created for the MIDI 2.0 device.

To learn more about endpoint discovery and protocol negotiation in MIDI 2.0, [see the UMP specification on the MIDI Association web site](https://midi.org/specs).

### Knowing when discovery has finished

`IsEndpointDiscoveryComplete` becomes true when discovery finishes or times out. The watcher then raises `Updated` with `IsEndpointDiscoveryStateUpdated` set. For an endpoint that doesn't use discovery in the protocol, such as a MIDI 1.0 device, it's true from the moment the endpoint is created.

It's a useful hint, not a promise. It stays false if discovery was stopped partway, for example because the device was unplugged. Don't make your application wait for it. And keep handling updates after you see it, because function block names and MIDI 1.0 port names in particular can still arrive later.

## Properties

| Property | Source | Description |
| --------------- | ------ | ----------- |
| `EndpointDeviceId` | Windows | The endpoint's device interface id, which you pass to `MidiSession.CreateEndpointConnection`. It's sometimes called "the SWD" for short, because it's the text that identifies the software device (SWD) interface for the endpoint |
| `Name` | Various | The name to show in your application. It picks the right name from all the names the endpoint has, including one the user set. Always respect the user's choice. The name can change at any time, so don't count on it staying the same between sessions, or even during one |
| `ContainerId` | Windows | The [device container GUID](https://learn.microsoft.com/windows-hardware/drivers/install/container-ids) |
| `DeviceInstanceId` | Windows | The [device instance id](https://learn.microsoft.com/windows-hardware/drivers/install/device-instance-ids) of the endpoint | 
| `EndpointPurpose` | Windows | What the endpoint is for. Mostly used for filtering |
| `ParentDeviceInstanceId` | Windows | The device instance id of the parent device |
| `DeclaredEndpointInfoLastUpdateTime` | Discovery | When the endpoint information from discovery last changed |
| `DeclaredDeviceIdentityLastUpdateTime` | Discovery | When the device identity from discovery last changed |
| `DeclaredStreamConfigurationLastUpdateTime` | Protocol Negotiation | When the stream configuration from protocol negotiation last changed |
| `DeclaredFunctionBlocksLastUpdateTime` | Discovery | When the function blocks last changed |
| `Midi1PortNamingApproach` | User/Config | How this endpoint's MIDI 1.0 port names are made |
| `IsMuted` | Config | True if this endpoint is muted, which means no MIDI messages get through |
| `IsEndpointDiscoveryComplete` | Discovery | True when the service has finished asking the device about itself. See [Knowing when discovery has finished](#knowing-when-discovery-has-finished) |
| `Properties` | Windows | The endpoint's raw device properties. Don't depend on these values or their ids. They're internal details that can change, and they aren't part of what the API promises. Everything useful in them is also available through the other properties and functions, with proper types |

## Functions

| Function | Description |
| --------------- | ----------- |
| `GetDeclaredEndpointInfo()` | Returns the saved endpoint information from discovery, as a `MidiDeclaredEndpointInfo` |
| `GetDeclaredDeviceIdentity()` | Returns the saved device identity from discovery, as a `MidiDeclaredDeviceIdentity` |
| `GetDeclaredStreamConfiguration()` | Returns the saved stream configuration, as a `MidiDeclaredStreamConfiguration` |
| `GetDeclaredFunctionBlocks()` | Returns a copy of the saved function blocks |
| `GetGroupTerminalBlocks()` | Returns the saved group terminal blocks. Only USB devices have these |
| `GetUserSuppliedInfo()` | Returns the saved information the user supplied, as a `MidiEndpointUserSuppliedInfo` |
| `GetTransportSuppliedInfo()` | Returns the saved information the transport supplied, as a `MidiEndpointTransportSuppliedInfo` |
| `GetParentDeviceInformation()` | Returns the parent device, as a `MidiParentDeviceInformation` |
| `GetContainerDeviceInformation()` | Returns the device container as a `Windows.Devices.Enumeration.DeviceInformation`, with the right properties filled in |
| `GetNameTable()` | Returns all the possible names for the MIDI 1.0 ports made from this UMP endpoint. Mostly used by MIDI Settings, so people can change the names of ports that will be created later |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `EndpointInterfaceClass` | The class GUID at the end of every endpoint id |

## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| `CreateFromEndpointDeviceId(endpointDeviceId)` | Creates a new `MidiEndpointDeviceInformation` for the endpoint with this id |
| `FindAll()` | Finds all endpoint devices and returns them in the default sort order |
| `FindAll(sortOrder)` | Finds all endpoint devices and returns them in the sort order you choose |
| `FindAll(sortOrder, endpointTypesToInclude)` | Finds all endpoint devices that match the filter, and returns them in the sort order you choose |
| `FindAllForContainer(containerId)` | Returns all endpoint devices in this device container |
| `DeviceMatchesFilter(deviceInformation, endpointTypesToInclude)` | Returns true if the device matches the filter |
| `GetAdditionalPropertiesList()` | Returns the list of extra properties to ask for when you enumerate devices yourself. Most applications don't need it, because the watcher calls it for you |

## Samples

* [C++/WinRT static-enum-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/static-enum-endpoints) and [C# static-enum-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/static-enum-endpoints) for a one-time list with function blocks and group terminal blocks
* [C++/WinRT get-vid-pid](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/get-vid-pid) and [C# get-vid-pid](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/get-vid-pid) for the parent device and transport-supplied metadata
* [C++/WinRT identify-endpoint-type](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/identify-endpoint-type) and [C# identify-endpoint-type](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/identify-endpoint-type) for working out what kind of device an endpoint is

If your application has a device picker, use [`MidiEndpointDeviceWatcher`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceWatcher/) instead of these static methods.
