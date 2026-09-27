---
layout: sdk_reference_page
title: MidiDeclaredDeviceIdentity
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Endpoint-supplied identification information from MIDI 2.0 discovery or SysEx Discovery.
---

The MIDI service fills this in during MIDI 2.0 endpoint discovery, from the device's Device Identity Notification message. When you get one from `MidiEndpointDeviceInformation`, it's read-only, and the `Set` methods do nothing.

A virtual device application creates one to describe its device, and a capability inquiry session can announce one in Discovery.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiDeclaredDeviceIdentity()` | Creates an empty identity |
| `MidiDeclaredDeviceIdentity(sysexIdByte1, sysexIdByte2, sysexIdByte3, deviceFamilyLsb, deviceFamilyMsb, deviceFamilyModelNumberLsb, deviceFamilyModelNumberMsb, softwareRevisionLevelByte1, softwareRevisionLevelByte2, softwareRevisionLevelByte3, softwareRevisionLevelByte4)` | Creates an identity with every value filled in |

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if this object is read-only. When it is, the `Set` methods do nothing |
| `SystemExclusiveId` | The manufacturer's three-byte System Exclusive id, as defined in the UMP specification |
| `DeviceFamilyLsb` | The least significant byte of the device family |
| `DeviceFamilyMsb` | The most significant byte of the device family |
| `DeviceFamilyModelNumberLsb` | The least significant byte of the model number within the device family |
| `DeviceFamilyModelNumberMsb` | The most significant byte of the model number within the device family |
| `SoftwareRevisionLevel` | The four-byte software version |

## Methods

| Method | Description |
| ------- | ----------- |
| `SetSystemExclusiveId(byte1, byte2, byte3)` | Sets the three-byte System Exclusive id |
| `SetDeviceFamily(deviceFamilyLsb, deviceFamilyMsb)` | Sets the device family bytes |
| `SetDeviceFamilyModelNumber(deviceFamilyModelNumberLsb, deviceFamilyModelNumberMsb)` | Sets the model number bytes |
| `SetSoftwareRevisionLevel(byte1, byte2, byte3, byte4)` | Sets the four-byte software version |

