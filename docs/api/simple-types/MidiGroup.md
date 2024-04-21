---
layout: api_page
title: MidiGroup
parent: Simple Types
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiGroup

## Remarks

The `MidiGroup` class is used to provide formatting and data validation for MIDI 2.0 groups. For clarity, the 0-15 value used in all messages is the `Index` and the 1-16 value those are mapped to for user display, is the `NumberForDisplay`.

## Constructors

| `MidiGroup` | Create an empty MidiGroup object (Index 0) |
| `MidiChannel(UInt8)` | Create a MidiChannel with the specified channel Index (0-15) |

## Properties

| `Index` | The data value, or channel Index (0-15) |
| `NumberForDisplay` | The number that should be displayed in any UI. (1-16) |

## Static Properties

| `LabelShort` | Returns the localized abbreviation. For example, "Gr" in English. |
| `LabelFull` | Returns the localized full name. For example, "Group" in English. |

## Static Methods

| `IsValidGroupIndex(UInt8)` | Verifies that the provided index is valid (between 0 and 15) |

## See also

[MidiGroup IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiGroup.idl)
