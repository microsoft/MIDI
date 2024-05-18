---
layout: api_page
title: MidiEndpointDeviceInformation
parent: Endpoint Enumeration
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiEndpointDeviceInformation

This class is a specialized equivalent of the `DeviceInformation` WinRT class. It handles requesting all of the additional properties necessary for MIDI devices, and also goes a step further to retrieve parent device information so that applications can display the endpoints and parent devices in context.

We've heard from developers that we did not provide sufficient information about devices in the past, so we created this class and the associated properties to remedy that. We also heard that Async calls were a non-starter for most DAW applications, so everything in this class is synchronous.

> Note: the MidiEndpointDeviceWatcher is a better way to retrieve devices because you can then keep the watcher open in a background thread, and be notified of property changes, device add/remove, etc.

When displaying endpoint devices to users, you'll typically want to stick to the defaults: `IncludeClientUmpNative | IncludeClientByteStreamNative`. You do not want to show the Diagnostic Ping ever, and you typically will not want to show the system-wide Diagnostic Loopback singletons. Finally, you don't want to show the Virtual Device Responder endpoints because those should be reserved only for the "device" application in app-to-app MIDI. 

## In-protocol discovered information

When a device is first enumerated by the MIDI Service, if it is a UMP-native device, we will attempt endpoint discover and protocol negotiation. During that, we request all endpoint information and all function block information. The received data is then cached in the device properties so that applications do not need to perform this process themselves.

## Properties

| Property | Source | Description |
| --------------- | ------ | ----------- |
| `Id` | Windows | The endpoint device interface id. This is sometimes called "the SWD" in short-hand because it's the string that uniquely identifies the software device interface that represents the endpoint. |
| `ContainerId` | Windows | The device container |
| `DeviceInstanceId` | Windows | The device instance id without the interface information | 
| `Name` | Various | This is the name which should be displayed in any application. It calculates the correct name based on the hierarchy of possible names, including a user-specified name. Always respect the user's choice here. |
| `TransportSuppliedName` | Transports | The name provided by the driver or the endpoint transport. |
| `EndpointSuppliedName` | MIDI 2.0 | The name provided by MIDI 2.0 endpoint information. This is discovered in-protocol. |
| `UserSuppliedName` | Configuration | The name provided by the user. |
| `ProductInstanceId` | MIDI 2.0 | Property of the same name discovered by MIDI 2.0 in-protocol endpoint information. |
| `SpecificationVersionMajor` | MIDI 2.0 | Discovered UMP version |
| `SpecificationVersionMinor` | MIDI 2.0 | Discovered UMP version |
| `SupportsMidi10Protocol` | MIDI 2.0 | Discovered protocol support. For all devices, this defaults to true. |
| `SupportsMidi20Protocol` | MIDI 2.0 | Discovered protocol support. For UMP-native devices, this defaults to true. |
| `ConfiguredToReceiveJRTimestamps | MIDI 2.0 | Note that JR timestamps are handled entirely in the service and are not sent back down to the client. |
| `ConfiguredToSendJRTimestamps | MIDI 2.0 | Note that JR timestamps are handled entirely in the service and are not sent back down to the client. |
| `DeviceIdentitySystemExclusiveId` | MIDI 2.0 | Device Identity information
| `DeviceIdentityDeviceFamilyLsb` | MIDI 2.0 | Device Identity information
| `DeviceIdentityDeviceFamilyMsb` | MIDI 2.0 | Device Identity information
| `DeviceIdentityDeviceFamilyModelNumberLsb` | MIDI 2.0 | Device Identity information
| `DeviceIdentityDeviceFamilyModelNumberMsb` | MIDI 2.0 | Device Identity information
| `DeviceIdentitySoftwareRevisionLevel` | MIDI 2.0 | Device Identity information
| `TransportId` | Windows | The Id of the transport abstraction that manages this endpoint |
| `TransportMnemonic` | Windows | A short abbreviation for the transport. This can be used as a transport identifier. |
| `TransportSuppliedSerialNumber` | Windows | iSerialNumber, when available in USB, and other ids from other transports. |
| `TransportSuppliedVendorId` | Windows | For a USB device, this is the VID of the parent device |
| `TransportSuppliedProductId` | Windows | For a USB device, this is the PID of the parent device |
| `ManufacturerName` | Windows | The name of the manufacturer of the device, if available |
| `SupportsMultiClient` | Windows | True if this endpoint supports multi-client use |
| `NativeDataFormat` | Windows | Because the driver and service handle data format translation, it's not immediately obvious if the device is natively UMP or natively Byte Stream. This property provides that information |
| `GroupTerminalBlocks` | Windows | A collection of Group Terminal Blocks. These are used only in USB. For MIDI 2.0 devices, Function Blocks are preferred. |
| `HasStaticFunctionBlocks` | MIDI 2.0 | True if the function blocks are static. That is, the groups never change. |
| `FunctionBlockCount` | MIDI 2.0 | The number of function blocks the endpoint has declared. Function blocks always start at index zero and go to FunctionBlockCount-1 |
| `EndpointPurpose` | Windows | The purpose of the endpoint. This is used primarily for filtering. |
| `Description` | Configuration | An endpoint description which is typically provided by the user |
| `LargeImagePath` | Configuration | The path to a png or jpg image that represents this endpoint. Typically user-supplied. |
| `SmallImagePath` | Configuration | The path to a png or jpg image that represents this endpoint. Typically user-supplied. |
| `RequiresNoteOffTranslation` | Configuration | True if the endpoint requires internal translation of Note On with zero velocity (in the case of MIDI 1.0) to a Note Off message. Typically user-supplied. |
| `RecommendedCCAutomationIntervalMS` | Configuration | Number of milliseconds between automation value changes. This is usually only for old and slow MIDI 1.0 devices that are prone to data flooding. User-supplied. |
| `Properties` | Windows | A collection of all the raw properties. |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `DiagnosticsLoopbackAEndpointId` | Endpoint Id for the diagnostic loopback used for development and support purposes. |
| `DiagnosticsLoopbackBEndpointId` | Endpoint Id for the diagnostic loopback used for development and support purposes. |
| `EndpointInterfaceClass` | The class GUID which appears at the end of the Endpoint Ids |

## Functions

| Function | Description |
| --------------- | ----------- |
| `GetParentDeviceInformation()` | Finds and then retrieves the parent `DeviceInformation` type with appropriate properties. |
| `GetContainerInformation()` | Gets the device container information and returns its `DeviceInformation` with appropriate properties |
| `UpdateFromDeviceInformation(deviceInformation)` | For use by any watcher which must update this object |
| `UpdateFromDeviceInformationUpdate(deviceInformationUpdate)` | For use by any watcher which must update this object |

## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| `CreateFromId(id)` | Creates a new `MidiEndpointDeviceInformation` object from the specified id |
| `FindAll()` | Searches for all endpoint devices and returns a list in the default sort order |
| `FindAll(sortOrder)` | Searches for all endpoint devices and returns a list in the specified sort order |
| `FindAll(sortOrder, endpointFilter)` | Searches for all endpoint devices which match the filter, and returns a list in the specified sort order. |
| `DeviceMatchesFilter(deviceInformation, endpointFilter)` | A helper function to compare a device against the filter. |
| `GetAdditionalPropertiesList()` | This returns the list of properties which must be requested during enumeration. Typically not needed for applications, as the watcher calls this function |

## IDL

[MidiEndpointDeviceInformation IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiEndpointDeviceInformation.idl)

## Sample

What follows is an excerpt of the larger C++/WinRT enumeration sample.

```cpp
#include "pch.h"
#include <iostream>

using namespace winrt::Windows::Devices::Midi2;        // API

int main()
{
    winrt::init_apartment();

    bool includeDiagnosticsEndpoints = true;

    // enumerate all endpoints. A normal application should enumerate only
    // IncludeClientByteStreamNative | IncludeClientUmpNative
    auto endpoints = MidiEndpointDeviceInformation::FindAll(
        MidiEndpointDeviceInformationSortOrder::Name,
        MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative |
        MidiEndpointDeviceInformationFilter::IncludeClientUmpNative |
        MidiEndpointDeviceInformationFilter::IncludeDiagnosticLoopback |
        MidiEndpointDeviceInformationFilter::IncludeVirtualDeviceResponder
    );

    std::cout << endpoints.Size() << " endpoints returned" << std::endl << std::endl;

    for (auto const& endpoint : endpoints)
    {
        std::cout << "Identification" << std::endl;
        std::cout << "- Name:    " << winrt::to_string(endpoint.Name()) << std::endl;
        std::cout << "- Id:      " << winrt::to_string(endpoint.Id()) << std::endl;

        std::cout << std::endl << "Endpoint Metadata" << std::endl;
        std::cout << "- Product Instance Id:    " << winrt::to_string(endpoint.ProductInstanceId()) << std::endl;
        std::cout << "- Endpoint-supplied Name: " << winrt::to_string(endpoint.EndpointSuppliedName()) << std::endl;
    
        std::cout << std::endl << "User-supplied Metadata" << std::endl;
        std::cout << "- User-supplied Name: " << winrt::to_string(endpoint.UserSuppliedName()) << std::endl;
        std::cout << "- Description:        " << winrt::to_string(endpoint.Description()) << std::endl;
        std::cout << "- Small Image Path:   " << winrt::to_string(endpoint.SmallImagePath()) << std::endl;
        std::cout << "- Large Image Path:   " << winrt::to_string(endpoint.LargeImagePath()) << std::endl;

        // ...

    }
}
```