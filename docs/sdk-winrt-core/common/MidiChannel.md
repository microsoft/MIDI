---
layout: api_page
title: MidiChannel
parent: Midi2 core
has_children: false
---

# MidiChannel

## Remarks

The `MidiChannel` class is used to provide formatting and data validation for MIDI 1.0 and MIDI 2.0 channels. For clarity, the 0-15 value used in all messages is the `Index` and the 1-16 value those are mapped to for user display, is the `DisplayValue`.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Implements

`IStringable`

## Constructors

| `MidiChannel(UInt8)` | Create a MidiChannel with the specified channel Index (0-15). Any data in the upper 4 bits of the provided byte is ignored, so you may pass in an entire status + channel value here without first cleaning it. |

## Properties

| `Index` | The data value, or channel Index (0-15) |
| `DisplayValue` | The number that should be displayed in any UI. (1-16) |

## Static Properties

| `ShortLabel` | Returns the localized abbreviation. For example, "Ch" in English. |
| `ShortLabelPlural` | Returns the localized abbreviation. For example, "Chs" in English. |
| `LongLabel` | Returns the localized full name. For example, "Channel" in English. |
| `LongLabelPlural` | Returns the localized full name. For example, "Channels" in English. |

## Static Methods

| `IsValidIndex(UInt8)` | Verifies that the provided index is valid (between 0 and 15) |

## Examples

More complete examples [available on Github](https://aka.ms/midirepo)

## IDL

[MidiChannel IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiChannel.idl)
