---
layout: sdk_reference_page
title: MidiDeclaredDeviceIdentity
namespace: Windows.Devices.Midi2.Enumeration
type: struct
description: Endpoint-supplied identification information from MIDI 2.0 discovery or SysEx Discovery.
---

This information is populated by the Windows Service during the MIDI 2.0 endpoint discovery process.

## Struct Fields

| Field | Description |
| --------------- | ----------- |
| `SystemExclusiveId` | Array of 3 bytes representing the System Exclusive Id per the MIDI 2.0 UMP spec |
| `DeviceFamilyLsb` | Least Significant Byte of the Device Family per the MIDI 2.0 UMP spec |
| `DeviceFamilyMsb` | Most Significant Byte of the Device Family per the MIDI 2.0 UMP spec |
| `DeviceFamilyModelNumberLsb` | Least Significant Byte of the Device Family Model Number per the MIDI 2.0 UMP spec |
| `DeviceFamilyModelNumberMsb` | Least Significant Byte of the Device Family Model Number per the MIDI 2.0 UMP spec |
| `SoftwareRevisionLevel` | Array of 4 bytes representing the Software Revision Level per the MIDI 2.0 UMP spec |

