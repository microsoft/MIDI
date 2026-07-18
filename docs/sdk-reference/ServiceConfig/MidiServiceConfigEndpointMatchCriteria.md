---
layout: sdk_reference_page
title: MidiServiceConfigEndpointMatchCriteria
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Criteria used to match a MIDI endpoint for configuration purposes
---

Used by `MidiServiceEndpointCustomizationConfig` to specify which endpoint the customization applies to.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiServiceConfigEndpointMatchCriteria()` | Create an empty match criteria object |

## Properties

| Property | Description |
| -------- | ----------- |
| `EndpointDeviceId` | Match by full endpoint device id |
| `DeviceInstanceId` | Match by device instance id |
| `UsbVendorId` | Match by USB vendor id |
| `UsbProductId` | Match by USB product id |
| `UsbSerialNumber` | Match by USB serial number string |
| `Midi2ProductInstanceId` | Match by MIDI 2.0 product instance id |
| `StaticIPAddress` | Match by static IP address (for network transport) |
| `Port` | Match by port number (for network transport) |
| `TransportSuppliedEndpointName` | Match by transport-supplied endpoint name |
| `ParentDeviceName` | Match by parent device name |

## Methods

| Method | Description |
| ------ | ----------- |
| `Matches(other)` | Returns true if this criteria matches the specified criteria |
| `GetConfigJson()` | Returns the JSON representation of this criteria |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `MatchObjectKey` | The JSON key used for the match object |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(matchObjectJson)` | Creates a `MidiServiceConfigEndpointMatchCriteria` from its JSON representation |
