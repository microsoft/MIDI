---
layout: sdk_reference_page
title: MidiEndpointTransportSuppliedInfo
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: Metadata for an endpoint supplied by the transport in the MIDI Service
---

## Struct Fields

| Field | Description |
| --------------- | ----------- |
| `Name` | The endpoint name as provided by the transport |
| `Description` | The description, if any, as provided by the transport |
| `SerialNumber` | Any unique serial number (iSerial in USB, for example) form the transport |
| `VendorId` | If the device is connected to the new UMP USB driver, or we can otherwise obtain it, this is the USB VID `idVendor` |
| `ProductId` | If the device is connected to the new UMP USB driver, or we can otherwise obtain it, this is the USB PID `idProduct` |
| `ManufacturerName` | If the device is connected to the new UMP USB driver, this is the manufactruer name from the USB headers |
| `SupportsMultiClient` | True if the endpoint supports multi-client use through Windows MIDI Services |
| `NativeDataFormat` | The `MidiEndpointNativeDataFormat` indicating if this device natively uses the MIDI 1.0 byte format, or the UMP format |
| `TransportId` | GUID identifying the transport in use |
| `TransportAbbreviation` | Short identifier for the transport, such as `KS` or `BLE` |
