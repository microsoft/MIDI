---
layout: sdk_reference_page
title: MidiDeclaredDeviceIdentity
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: Endpoint-supplied identification information from MIDI 2.0 discovery or SysEx Discovery.
---

This information is populated by the Windows Service during the MIDI 2.0 endpoint discovery process.

## Struct Fields

| Field | Description |
| --------------- | ----------- |
| `SystemExclusiveIdByte1` | Byte 1 of the System Exclusive Id per the MIDI 2.0 UMP spec |
| `SystemExclusiveIdByte2` | Byte 2 of the System Exclusive Id per the MIDI 2.0 UMP spec |
| `SystemExclusiveIdByte3` | Byte 3 of the System Exclusive Id per the MIDI 2.0 UMP spec |
| `DeviceFamilyLsb` | Least Significant Byte of the Device Family per the MIDI 2.0 UMP spec |
| `DeviceFamilyMsb` | Most Significant Byte of the Device Family per the MIDI 2.0 UMP spec |
| `DeviceFamilyModelNumberLsb` | Least Significant Byte of the Device Family Model Number per the MIDI 2.0 UMP spec |
| `DeviceFamilyModelNumberMsb` | Least Significant Byte of the Device Family Model Number per the MIDI 2.0 UMP spec |
| `SoftwareRevisionLevelByte1` | Byte 1 of the Software Revision Level per the MIDI 2.0 UMP spec |
| `SoftwareRevisionLevelByte2` | Byte 2 of the Software Revision Level per the MIDI 2.0 UMP spec |
| `SoftwareRevisionLevelByte3` | Byte 3 of the Software Revision Level per the MIDI 2.0 UMP spec |
| `SoftwareRevisionLevelByte4` | Byte 4 of the Software Revision Level per the MIDI 2.0 UMP spec |

