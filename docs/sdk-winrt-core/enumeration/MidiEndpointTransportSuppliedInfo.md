---
layout: api_page
title: MidiEndpointTransportSuppliedInfo
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointTransportSuppliedInfo

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Struct Fields

| Field | Description |
| --------------- | ----------- |
| `Name` | The endpoint name as provided by the transport |
| `Description` | The description, if any, as provided by the transport |
| `SerialNumber` | Any unique serial number (iSerial in USB, for example) form the transport |
| `VendorId` | If the device is connected to the new UMP USB driver, this is the USB VID |
| `ProductId` | If the device is connected to the new UMP USB driver, this is the USB PID |
| `ManufacturerName` | If the device is connected to the new UMP USB driver, this is the manufactruer name from the USB headers |
| `SupportsMultiClient` | True if the endpoint supports multi-client use through Windows MIDI Services |
| `NativeDataFormat` | The `MidiEndpointNativeDataFormat` indicating if this device natively uses the MIDI 1.0 byte format, or the UMP format |
| `TransportId` | GUID identifying the transport in use |
| `TransportAbbreviation` | Short identifier for the transport, such as `KS` or `BLE` |

## IDL

[MidiEndpointTransportSuppliedInfo IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiEndpointTransportSuppliedInfo.idl)

