---
layout: sdk_reference_page
title: MidiChannel
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Class used to provide formatting and data validation for MIDI 1.0 and MIDI 2.0 channels.
---

The `MidiChannel` class is used to provide formatting and data validation for MIDI 1.0 and MIDI 2.0 channels. For clarity, the 0-15 value used in all messages is the `Index` and the 1-16 value those are mapped to for user display, is the `DisplayValue`.

## Constructors

| `MidiChannel()` | Create a MidiChannel with index 0 |
| `MidiChannel(UInt8)` | Create a MidiChannel with the specified channel Index (0-15). Any data in the upper 4 bits of the provided byte is ignored, so you may pass in an entire status + channel value here without first cleaning it. C++ note: C++/WinRT creates a constructor which takes nullptr, as a result `MidiChannel(0)` will fail to compile if you have the compiler option set to equate 0 and nullptr. To avoid this, use `MidiChannel(static_cast<uint8_t>(0))` or simply `MidiChannel()`|

## Properties

| `Index` | The data value, or channel Index (0-15) |
| `DisplayValue` | The number that should be displayed in any UI. (1-16) |

## Static Properties

| `ShortLabel` | Returns the localized abbreviation. For example, "Ch" in English. |
| `ShortLabelPlural` | Returns the localized plural abbreviation. |
| `LongLabel` | Returns the localized full name. For example, "Channel" in English. |
| `LongLabelPlural` | Returns the localized full plural name. |

## Static Methods

| `IsValidIndex(UInt8)` | Verifies that the provided index is valid (between 0 and 15) |

## Examples

More complete examples [available on Github](https://aka.ms/midirepo)
