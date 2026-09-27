---
layout: sdk_reference_page
title: MidiParentDeviceInformation
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Information about the parent device of a MIDI endpoint
---

Details about the parent device of a MIDI endpoint, such as a USB device. `MidiEndpointDeviceInformation.GetParentDeviceInformation()` returns one.

## Properties

| Property | Description |
| -------- | ----------- |
| `Id` | The device instance id of the parent device |
| `Name` | The friendly name of the parent device |
| `ContainerId` | The container GUID for this device |
| `ParentDeviceInstanceId` | The device instance id of this device's own parent |
| `RelatedParentMediaDriverDeviceInstanceId` | The device instance id of the related parent media driver device, if there is one |
| `DriverInfPath` | The path to the driver's INF file |
| `DriverKey` | The driver's registry key |
| `DriverProvider` | The name of the company that provided the driver |
| `ServiceName` | The name of the driver service for this device |
| `DriverVersion` | The driver's version |
| `EnumeratorName` | The name of the device enumerator, such as "USB" |
| `UsbVendorId` | The USB vendor id (VID), for a USB device |
| `UsbProductId` | The USB product id (PID), for a USB device |
| `UsbSerialNumber` | The USB serial number, for a USB device that has one |
| `ReportedDeviceIdsHash` | A hash of the device ids the device reported, for quick comparisons |

## Samples

These show where to find the USB vendor and product ids, and how this type is different from the ids the transport reports. With WinMM, you would have used `DRV_QUERYDEVICEINTERFACE` and read the ids out of the interface string yourself.

* [C++/WinRT get-vid-pid](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/get-vid-pid)
* [C# get-vid-pid](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/get-vid-pid)
