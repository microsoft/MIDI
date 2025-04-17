---
layout: sdk_reference_page
title: MidiGroup
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Represents a MIDI 2.0 group and performs data validation per the specification
---

The `MidiGroup` class is used to provide formatting and data validation for MIDI 2.0 groups. For clarity, the 0-15 value used in all messages is the `Index` and the 1-16 value those are mapped to for user display, is the `DisplayValue`.

## Constructors

| `MidiGroup()` | Create a MidiGroup initialized with group Index 0 |
| `MidiGroup(UInt8)` | Create a MidiGroup with the specified group Index (0-15). C++ note: C++/WinRT creates a constructor which takes nullptr, as a result `MidiGroup(0)` will fail to compile if you have the compiler option set to equate 0 and nullptr. To avoid this, use `MidiGroup(static_cast<uint8_t>(0))` or simply `MidiGroup()` |

## Properties

| `Index` | The data value, or group Index (0-15) |
| `DisplayValue` | The number that should be displayed in any UI. (1-16) |

## Static Properties

| `ShortLabel` | Returns the localized abbreviation. For example, "Gr" in English. |
| `ShortLabelPlural` | Returns the localized plural abbreviation. |
| `LongLabel` | Returns the localized full name. For example, "Group" in English. |
| `LongLabelPlural` | Returns the localized full plural name.|

## Static Methods

| `IsValidIndex(UInt8)` | Verifies that the provided index is valid (between 0 and 15) |
