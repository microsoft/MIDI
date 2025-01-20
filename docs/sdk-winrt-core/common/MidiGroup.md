---
layout: api_page
title: MidiGroup
parent: Midi2 core
has_children: false
---

# MidiGroup

## Remarks

The `MidiGroup` class is used to provide formatting and data validation for MIDI 2.0 groups. For clarity, the 0-15 value used in all messages is the `Index` and the 1-16 value those are mapped to for user display, is the `DisplayValue`.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Implements

`IStringable`

## Constructors

| `MidiGroup(UInt8)` | Create a MidiGroup with the specified group Index (0-15) |

## Properties

| `Index` | The data value, or group Index (0-15) |
| `DisplayValue` | The number that should be displayed in any UI. (1-16) |

## Static Properties

| `ShortLabel` | Returns the localized abbreviation. For example, "Gr" in English. |
| `ShortLabelPlural` | Returns the localized abbreviation. For example, "Grs" in English. |
| `LongLabel` | Returns the localized full name. For example, "Group" in English. |
| `LongLabelPlural` | Returns the localized full name. For example, "Groups" in English. |

## Static Methods

| `IsValidIndex(UInt8)` | Verifies that the provided index is valid (between 0 and 15) |

## See also

[MidiGroup IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiGroup.idl)
