---
layout: sdk_reference_page
title: MidiGroup
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Represents a MIDI 2.0 group and performs data validation per the specification
---

`MidiGroup` holds a MIDI 2.0 group number and keeps it in range. Messages number groups from 0 to 15, and that's the `Index`. People see groups numbered from 1 to 16, and that's the `DisplayValue`. Show `DisplayValue` anywhere a person will read the group number.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiGroup()` | Creates a group with index 0 |
| `MidiGroup(index)` | Creates a group with this index, from 0 to 15. Only the lower 4 bits are used. **C++ note:** C++/WinRT also creates a constructor that takes `nullptr`, so `MidiGroup(0)` won't compile if your compiler settings treat `0` as `nullptr`. Use `MidiGroup(static_cast<uint8_t>(0))` or `MidiGroup()` instead |

## Properties

| Property | Description |
| -------- | ----------- |
| `Index` | The group number used in messages, from 0 to 15. When you set it, only the lower 4 bits are kept |
| `DisplayValue` | The group number to show people, from 1 to 16 |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ShortLabel` | The short word for "group" in the user's language. For example, "Gr" in English |
| `ShortLabelPlural` | The plural of the short word |
| `LongLabel` | The full word in the user's language. For example, "Group" in English |
| `LongLabelPlural` | The plural of the full word |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `IsValidIndex(index)` | Returns true if the index is from 0 to 15 |

## Methods

| Method | Description |
| ------ | ----------- |
| `ToString()` | (From `IStringable`) The group as text, for display |
