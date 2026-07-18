---
layout: sdk_reference_page
title: MidiEndpointTransportSuppliedInfo
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Metadata for an endpoint supplied by the transport in the MIDI Service
---

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if this object should be treated as read-only |
| `Name` | The endpoint name as provided by the transport |
| `Description` | The description, if any, as provided by the transport |
| `SerialNumber` | Any unique serial number (iSerial in USB, for example) from the transport |
| `VendorId` | If the device is connected to the new UMP USB driver, or we can otherwise obtain it, this is the USB VID `idVendor` |
| `ProductId` | If the device is connected to the new UMP USB driver, or we can otherwise obtain it, this is the USB PID `idProduct` |
| `ManufacturerName` | If the device is connected to the new UMP USB driver, this is the manufacturer name from the USB headers |
| `SupportsMultiClient` | True if the endpoint supports multi-client use through Windows MIDI Services |
| `NativeDataFormat` | The `MidiEndpointNativeDataFormat` indicating if this device natively uses the MIDI 1.0 byte format, or the UMP format |
| `TransportId` | GUID identifying the transport in use |
| `TransportCode` | Short identifier for the transport, such as `KS` or `BLE` |
| `DriverDeviceInterfaceId` | The driver device interface id for this endpoint, if applicable |
