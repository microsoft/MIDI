---
layout: sdk_reference_page
title: MidiServiceConfigEndpointMatchCriteria
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Criteria used to match a MIDI endpoint for configuration purposes
---

Says which endpoint a customization applies to. `MidiServiceEndpointCustomizationConfig`, `MidiServiceEndpointCustomization`, and `MidiServiceEndpointCustomizationRemovalConfig` all use it.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiServiceConfigEndpointMatchCriteria()` | Creates empty match criteria. Then set the properties you want to match on |

## Properties

| Property | Description |
| -------- | ----------- |
| `EndpointDeviceId` | Match on the full endpoint device id |
| `DeviceInstanceId` | Match on the device instance id |
| `UsbVendorId` | Match on the USB vendor id |
| `UsbProductId` | Match on the USB product id |
| `UsbSerialNumber` | Match on the USB serial number |
| `Midi2ProductInstanceId` | Match on the MIDI 2.0 product instance id |
| `StaticIPAddress` | Match on a static IP address, for the network transport |
| `Port` | Match on a port number, for the network transport |
| `TransportSuppliedEndpointName` | Match on the name the transport gave the endpoint |
| `ParentDeviceName` | Match on the parent device's name |

## Methods

| Method | Description |
| ------ | ----------- |
| `Matches(other)` | Returns true if these criteria match `other` |
| `GetConfigJson()` | Returns these criteria as a JSON string |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `MatchObjectKey` | The JSON key the match criteria are stored under |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromJson(matchObjectJson)` | Creates match criteria from a JSON string, like the one `GetConfigJson` returns |
