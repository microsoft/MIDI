---
layout: sdk_reference_page
title: MidiParentDeviceInformation
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Information about the parent device of a MIDI endpoint
---

This class provides detailed information about the parent device of a MIDI endpoint. It is returned by `MidiEndpointDeviceInformation.GetParentDeviceInformation()` and similar methods.

## Properties

| Property | Description |
| -------- | ----------- |
| `Id` | The device instance id of the parent device |
| `Name` | The friendly name of the parent device |
| `ContainerId` | The container GUID for this device |
| `ParentDeviceInstanceId` | The device instance id of the parent's parent device |
| `RelatedParentMediaDriverDeviceInstanceId` | Device instance id of any related parent media driver device |
| `DriverInfPath` | Path to the driver INF file |
| `DriverKey` | The driver registry key |
| `DriverProvider` | The name of the driver provider |
| `ServiceName` | The name of the logical service associated with this device |
| `DriverVersion` | The version string for the driver |
| `EnumeratorName` | The name of the device enumerator (e.g., "USB") |
| `UsbVendorId` | USB vendor id (VID), if applicable |
| `UsbProductId` | USB product id (PID), if applicable |
| `UsbSerialNumber` | USB serial number string, if applicable |
| `ReportedDeviceIdsHash` | Hash of reported device ids for quick comparison |
