---
layout: api_page
title: MidiChannel
parent: Simple Types
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiChannel

## Remarks

The `MidiChannel` class is used to provide formatting and data validation for MIDI 1.0 and MIDI 2.0 channels. For clarity, the 0-15 value used in all messages is the `Index` and the 1-16 value those are mapped to for user display, is the `NumberForDisplay`.

## Constructors

| `MidiChannel` | Create an empty MidiChannel object (Index 0) |
| `MidiChannel(UInt8)` | Create a MidiChannel with the specified channel Index (0-15) |

## Properties

| `Index` | The data value, or channel Index (0-15) |
| `NumberForDisplay` | The number that should be displayed in any UI. (1-16) |

## Static Properties

| `ShortLabel` | Returns the localized abbreviation. For example, "Ch" in English. |
| `LongLabel` | Returns the localized full name. For example, "Channel" in English. |

## Static Methods

| `IsValidChannelIndex(UInt8)` | Verifies that the provided index is valid (between 0 and 15) |

## See also

[MidiChannel IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiChannel.idl)
