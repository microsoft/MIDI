---
layout: sdk_reference_page
title: MidiEndpointTransportSuppliedInfo
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Metadata for an endpoint supplied by the transport in the MIDI Service
---

What the transport knows about an endpoint, such as the name Windows has for the device, its USB ids, and how it's connected. When you get one from `MidiEndpointDeviceInformation`, treat it as read-only. Changing it doesn't change the endpoint.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiEndpointTransportSuppliedInfo()` | Creates an empty object |
| `MidiEndpointTransportSuppliedInfo(name, description, serialNumber, vendorId, productId, manufacturerName, supportsMultiClient, nativeDataFormat, transportId, transportCode, driverDeviceInterfaceId)` | Creates an object with every value filled in |

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if you should treat this object as read-only |
| `Name` | The endpoint name from the transport |
| `Description` | The description from the transport, if there is one |
| `SerialNumber` | A serial number from the transport, if there is one, such as `iSerial` for a USB device |
| `VendorId` | The USB vendor id (`idVendor`), when the device uses the new UMP USB driver or the id is available another way |
| `ProductId` | The USB product id (`idProduct`), when the device uses the new UMP USB driver or the id is available another way |
| `ManufacturerName` | The manufacturer name from the USB descriptors, when the device uses the new UMP USB driver |
| `SupportsMultiClient` | True if more than one application can use the endpoint at the same time through Windows MIDI Services |
| `NativeDataFormat` | A `MidiEndpointNativeDataFormat` that says whether the device itself uses the MIDI 1.0 byte format or UMP |
| `TransportId` | A GUID that identifies the transport |
| `TransportCode` | A short code for the transport, such as `KS` or `BLE` |
| `DriverDeviceInterfaceId` | The driver's device interface id for this endpoint, if it has one |

## Samples

`TransportId` and `TransportCode` tell you what kind of endpoint this really is, which WinMM never could. `VendorId` and `ProductId` here come from the transport, and aren't the same as the parent device's ids.

* [C++/WinRT identify-endpoint-type](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/identify-endpoint-type)
* [C# identify-endpoint-type](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/identify-endpoint-type)
* [C++/WinRT get-vid-pid](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/get-vid-pid)
* [C# get-vid-pid](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/get-vid-pid)
