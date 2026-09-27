---
layout: sdk_reference_page
title: MidiChannel
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Class used to provide formatting and data validation for MIDI 1.0 and MIDI 2.0 channels.
---

`MidiChannel` holds a MIDI 1.0 or MIDI 2.0 channel number and keeps it in range. Messages number channels from 0 to 15, and that's the `Index`. People see channels numbered from 1 to 16, and that's the `DisplayValue`. Show `DisplayValue` anywhere a person will read the channel number.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiChannel()` | Creates a channel with index 0 |
| `MidiChannel(index)` | Creates a channel with this index, from 0 to 15. Only the lower 4 bits are used, so you can pass a whole MIDI 1.0 status byte, such as `0x93`, without masking it first. **C++ note:** C++/WinRT also creates a constructor that takes `nullptr`, so `MidiChannel(0)` won't compile if your compiler settings treat `0` as `nullptr`. Use `MidiChannel(static_cast<uint8_t>(0))` or `MidiChannel()` instead |

## Properties

| Property | Description |
| -------- | ----------- |
| `Index` | The channel number used in messages, from 0 to 15. When you set it, only the lower 4 bits are kept |
| `DisplayValue` | The channel number to show people, from 1 to 16 |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ShortLabel` | The short word for "channel" in the user's language. For example, "Ch" in English |
| `ShortLabelPlural` | The plural of the short word |
| `LongLabel` | The full word in the user's language. For example, "Channel" in English |
| `LongLabelPlural` | The plural of the full word |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `IsValidIndex(index)` | Returns true if the index is from 0 to 15 |

## Methods

| Method | Description |
| ------ | ----------- |
| `ToString()` | (From `IStringable`) The channel as text, for display |

## Examples

More complete examples are [available on GitHub](https://aka.ms/midirepo)
