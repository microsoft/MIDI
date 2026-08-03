---
layout: sdk_reference_page
title: MidiUniversalSystemExclusiveChannel
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Class used to provide formatting and data validation for Universal System Exclusive (SysEx 8) channels.
---

The `MidiUniversalSystemExclusiveChannel` class is used to provide formatting and data validation for Universal System Exclusive (SysEx 7) channel types used in MIDI 2.0.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiUniversalSystemExclusiveChannel()` | Create a `MidiUniversalSystemExclusiveChannel` with index 0 |
| `MidiUniversalSystemExclusiveChannel(UInt8)` | Create a `MidiUniversalSystemExclusiveChannel` with the specified channel index |

## Properties

| Property | Description |
| -------- | ----------- |
| `Index` | The channel index value |
| `DisplayValue` | The number that should be displayed in any UI (Index + 1) |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ShortLabel` | Returns the localized abbreviation. |
| `ShortLabelPlural` | Returns the localized plural abbreviation. |
| `LongLabel` | Returns the localized full name. |
| `LongLabelPlural` | Returns the localized full plural name. |
| `DisregardChannel` | Returns a `MidiUniversalSystemExclusiveChannel` instance representing the "disregard channel" value per the MIDI 2.0 specification. |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `IsValidIndex(UInt8)` | Verifies that the provided index is valid. |
