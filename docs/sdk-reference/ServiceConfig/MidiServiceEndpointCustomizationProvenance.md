---
layout: sdk_reference_page
title: MidiServiceEndpointCustomizationProvenance
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Describes the device a stored endpoint customization was created for
---

An endpoint identifier can change, and when it does the stored customization stops matching anything. Provenance is what lets a customer recognize their own entry afterwards, so they can put it back on the right device.

None of it takes part in matching. Which endpoint a customization applies to is decided by `MidiServiceConfigEndpointMatchCriteria` and nothing else. Vendor and product identifiers are recorded here precisely because they must not be matched on: several devices can share them, and some report the same placeholder serial number as each other.

An application which customizes an endpoint should set this, so that the entry can be recovered later.

## Static members

| Member | Description |
| ------ | ----------- |
| `ProvenanceObjectKey` | The configuration file key this object is stored under |
| `CreateForEndpoint(endpointDeviceInformation)` | Fills in everything a present endpoint can supply and stamps the creation time |

## Properties

| Property | Description |
| -------- | ----------- |
| `CreatedFor` | What the customer would call the device. Their own name for it if they had already set one, otherwise the transport-supplied name |
| `Created` | When the entry was first created, as an ISO 8601 UTC timestamp |
| `UsbVendorId` | USB vendor identifier, when the device is a USB device |
| `UsbProductId` | USB product identifier |
| `UsbSerialNumber` | The serial number the device reported, which may be blank or a placeholder |
| `ManufacturerName` | The manufacturer name the device reported |
| `TransportSuppliedName` | The name the transport gave the endpoint at the time |
| `ParentDeviceInstanceId` | The device instance id of the endpoint's parent |
| `LatencySource` | Whether a stored outgoing latency was typed in or measured |
| `LatencyMeasured` | When a measured latency was taken, as an ISO 8601 UTC timestamp |
| `GetConfigJson` | The configuration file representation of this object |

## Preserving history across an edit

When updating a customization which already has provenance, refresh the device facts from whichever endpoint the entry now belongs to, but keep `Created` and the two latency fields. The creation date is the customer's own history and a measurement record tells an interface whether a stored latency can be cheaply reproduced or is irreplaceable.

## Latency source

`LatencySource` exists so that anything offering to discard a customization can say which kind of value it is about to throw away. A typed latency can be retyped. A measured one needed a loopback cable, a measurement and the patience to run it, and no customer will reproduce it from memory.
