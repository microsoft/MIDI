---
layout: sdk_reference_page
title: MidiUniversalSystemExclusiveChannel
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Holds and checks the channel number in a Universal System Exclusive message
---

A Universal System Exclusive message carries a number from 0 to 127 that says which device or channel it's meant for. `MidiUniversalSystemExclusiveChannel` holds that number and keeps it in range. The value 127 means "every device." The MIDI specification calls it "disregard channel," and `DisregardChannel` returns it.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiUniversalSystemExclusiveChannel()` | Creates a channel with index 0 |
| `MidiUniversalSystemExclusiveChannel(index)` | Creates a channel with this index, from 0 to 127. Only the lower 7 bits are used |

## Properties

| Property | Description |
| -------- | ----------- |
| `Index` | The channel number used in messages, from 0 to 127. When you set it, only the lower 7 bits are kept |
| `DisplayValue` | The number to show people, which is `Index` + 1 |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ShortLabel` | The short name for this kind of channel in the user's language |
| `ShortLabelPlural` | The plural of the short name |
| `LongLabel` | The full name in the user's language |
| `LongLabelPlural` | The plural of the full name |
| `DisregardChannel` | Returns a channel with index 127, which means the message is for every device |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `IsValidIndex(index)` | Returns true if the index is from 0 to 127 |

## Methods

| Method | Description |
| ------ | ----------- |
| `ToString()` | (From `IStringable`) The channel as text, for display |
