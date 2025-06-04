---
layout: sdk_reference_page
title: MidiUniqueId
namespace: Microsoft.Windows.Devices.Midi2.CapabilityInquiry
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Windows.Foundation.IStringable
description: A MIDI 2.0 MUID
---

The `MidiUniqueId` class is used to provide formatting and data validation for MIDI-CI MUID (MIDI Capability Inquiry Unique Id) types used in Function Blocks and MIDI CI transactions.

In the specification, Byte1 is the LSB and Byte4 is the MSB. We follow that convention here.

## Constructors

| `MidiUniqueId()` | Constructs an empty `MidiUniqueId` |
| `MidiUniqueId (UInt32)` | Constructs the `MidiUniqueId` from the given 28 bit integer |
| `MidiUniqueId (UInt8, UInt8, UInt8, UInt8)` | Constructs a `MidiUniqueId` with the specified seven-bit bytes |

## Properties

| `Byte1` | The data value for byte 1 of the MUID (LSB) |
| `Byte2` | The data value for byte 2 of the MUID (second-most LSB) |
| `Byte3` | The data value for byte 3 of the MUID (second-most MSB) |
| `Byte4` | The data value for byte 4 of the MUID (MSB) |
| `AsCombined28BitValue` | The data value converted to a 28 bit integer |
| `IsBroadcast` | True if this is the broadcast MUID value from the MIDI CI specification |
| `IsReserved` | True if this is the reserved MUID value from the MIDI CI specification |

## Methods

| `ToString` | (From `IStringable`) provides a string representation of the values in this type |

## Static Properties

| `ShortLabel` | Returns the localized abbreviation for use in UI. |
| `ShortLabelPlural` | Returns the localized abbreviation for use in UI. |
| `LongLabel` | Returns the localized full name for use in UI. |
| `LongLabelPlural` | Returns the localized full name for use in UI. |

## Static Methods

| `CreateBroadcast()` | Constructs a broadcast `MidiUniqueId` per the MIDI CI specification |
| `CreateRandom()` | Constructs a random `MidiUniqueId` per the MIDI CI specification |
