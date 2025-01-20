---
layout: api_page
title: MidiDeclaredDeviceIdentity
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiDeclaredDeviceIdentity

This information is populated by the Windows Service during the MIDI 2.0 endpoint discovery process.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

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

## IDL

[MidiDeclaredDeviceIdentity IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiDeclaredDeviceIdentity.idl)


