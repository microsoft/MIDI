---
layout: sdk_reference_page
title: MidiServiceEndpointCustomizationProvenance
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Describes the device a stored endpoint customization was created for
---

An endpoint's id can change. When it does, its stored customization stops matching anything. Provenance, a record of where the entry came from, lets people recognize their own entry afterward, so they can put it back on the right device.

None of it is used for matching. Only `MidiServiceConfigEndpointMatchCriteria` decides which endpoint a customization applies to. The vendor and product ids are kept here because they must not be matched on. Several devices can share them, and some devices even report the same placeholder serial number.

An application that customizes an endpoint should set this, so the entry can be recovered later.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiServiceEndpointCustomizationProvenance()` | Creates an empty provenance. Most applications use `CreateForEndpoint` instead |

## Static members

| Member | Description |
| ------ | ----------- |
| `ProvenanceObjectKey` | The key this object is stored under in the configuration |
| `CreateForEndpoint(endpointDeviceInformation)` | Fills in everything an endpoint that's present can supply, and records the creation time |

## Properties

| Property | Description |
| -------- | ----------- |
| `CreatedFor` | What the person would call the device: their own name for it if they'd already set one, and otherwise the name the transport supplied |
| `Created` | When the entry was first created, as an ISO 8601 UTC timestamp |
| `UsbVendorId` | The USB vendor id, when it's a USB device |
| `UsbProductId` | The USB product id |
| `UsbSerialNumber` | The serial number the device reported. It may be blank or a placeholder |
| `ManufacturerName` | The manufacturer name the device reported |
| `TransportSuppliedName` | The name the transport gave the endpoint at the time |
| `ParentDeviceInstanceId` | The device instance id of the endpoint's parent device |
| `LatencySource` | Whether a stored outgoing latency was typed in or measured |
| `LatencyMeasured` | When a measured latency was taken, as an ISO 8601 UTC timestamp |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetConfigJson()` | Returns this object as a JSON string, the way it's stored in the configuration |

## Keeping history when you edit

When you update a customization that already has provenance, refresh the device details from the endpoint the entry belongs to now. But keep `Created` and the two latency fields. The creation date is part of the person's own history. And the measurement record tells your app whether a stored latency is easy to reproduce or can't be replaced.

## Latency source

`LatencySource` lets anything that offers to delete a customization say what kind of value it's about to throw away. A typed latency can be typed again. A measured one took a loopback cable, a measurement, and the patience to run it. Nobody can reproduce it from memory.
