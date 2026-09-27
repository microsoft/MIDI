---
layout: sdk_reference_page
title: MidiUniqueId
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IStringable
description: A MIDI-CI unique identifier (MUID)
---

A MUID (MIDI unique identifier) is the 28-bit number a device answers to in MIDI-CI messages and function blocks. `MidiUniqueId` holds one, keeps each of its four bytes to 7 bits, and formats it for display.

The specification calls the least significant byte `Byte1` and the most significant byte `Byte4`, and this class does the same.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiUniqueId()` | Creates an empty MUID |
| `MidiUniqueId(combined28BitValue)` | Creates a MUID from a 28-bit number |
| `MidiUniqueId(sevenBitByte1, sevenBitByte2, sevenBitByte3, sevenBitByte4)` | Creates a MUID from four 7-bit bytes, least significant first |

## Properties

| Property | Description |
| -------- | ----------- |
| `Byte1` | Byte 1 of the MUID, the least significant. When you set a byte, only its lower 7 bits are kept |
| `Byte2` | Byte 2 of the MUID |
| `Byte3` | Byte 3 of the MUID |
| `Byte4` | Byte 4 of the MUID, the most significant |
| `AsCombined28BitValue` | The MUID as one 28-bit number |
| `IsBroadcast` | True if this is the broadcast MUID, `0x0FFFFFFF`, which is addressed to every device |
| `IsReserved` | True if this is one of the MUIDs the MIDI-CI specification sets aside, from `0x0FFFFF00` to `0x0FFFFFFE` |

## Methods

| Method | Description |
| ------ | ----------- |
| `ToString` | (From `IStringable`) The MUID as text, for display |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ShortLabel` | The short name for a MUID in the user's language |
| `ShortLabelPlural` | The plural of the short name |
| `LongLabel` | The full name in the user's language |
| `LongLabelPlural` | The plural of the full name |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `CreateBroadcast()` | Creates the broadcast MUID, which is addressed to every device |
| `CreateRandom()` | Creates a random MUID, as the MIDI-CI specification requires. It's never the broadcast MUID or a reserved one |
