---
layout: sdk_reference_page
title: MidiDeclaredDeviceIdentity
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Endpoint-supplied identification information from MIDI 2.0 discovery or SysEx Discovery.
---

This information is populated by the Windows Service during the MIDI 2.0 endpoint discovery process. When retrieved from a `MidiEndpointDeviceInformation` object, it is read-only.

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if this object should be treated as read-only |
| `SystemExclusiveId` | Array of 3 bytes representing the System Exclusive Id per the MIDI 2.0 UMP spec |
| `DeviceFamilyLsb` | Least Significant Byte of the Device Family per the MIDI 2.0 UMP spec |
| `DeviceFamilyMsb` | Most Significant Byte of the Device Family per the MIDI 2.0 UMP spec |
| `DeviceFamilyModelNumberLsb` | Least Significant Byte of the Device Family Model Number per the MIDI 2.0 UMP spec |
| `DeviceFamilyModelNumberMsb` | Most Significant Byte of the Device Family Model Number per the MIDI 2.0 UMP spec |
| `SoftwareRevisionLevel` | Array of 4 bytes representing the Software Revision Level per the MIDI 2.0 UMP spec |

## Methods

| Method | Description |
| ------- | ----------- |
| `SetSystemExclusiveId(byte1, byte2, byte3)` | Sets the three-byte System Exclusive Id |
| `SetDeviceFamily(deviceFamilyLsb, deviceFamilyMsb)` | Sets the Device Family bytes |
| `SetDeviceFamilyModelNumber(lsb, msb)` | Sets the Device Family Model Number bytes |
| `SetSoftwareRevisionLevel(byte1, byte2, byte3, byte4)` | Sets the four-byte Software Revision Level |

