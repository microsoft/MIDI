---
layout: sdk_reference_page
title: MidiGroupTerminalBlock
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
implements: Windows.Foundation.IStringable
description: An optional and static definition of the use of groups for an endpoint
---

A group terminal block describes the groups on a USB device. It exists only for USB. When a device has function blocks, use them instead to find its active groups, names, and more, and you can usually ignore its group terminal blocks. To learn more, see [MidiFunctionBlock]({{ site.baseurl }}/sdk-reference/Enumeration/MidiFunctionBlock/).

> **Note:** Windows MIDI Services turns each "port" on a MIDI 1.0 device into its own group terminal block. Each virtual cable number, which used to become a separate input or output port, now becomes a group number. For example, a MIDI 1.0 device with 5 ports shows up as one endpoint with 5 group terminal blocks, each covering one group.

## Properties

| Property | Description |
| --------------- | ----------- |
| `Number` | The block number |
| `Name` | The name from the USB descriptors. For a MIDI 1.0 device, this is the `iJack` string, if the device has one |
| `Direction` | Which way messages go, from the block's point of view |
| `Protocol` | Which protocol the block uses. Ignore the jitter reduction values here. Jitter reduction timestamps are agreed on during protocol negotiation, and the service handles them completely |
| `FirstGroup` | The first group this block covers |
| `GroupCount` | How many groups this block covers |
| `MaxDeviceInputBandwidthIn4KBitsPerSecondUnits` | The device's maximum input bandwidth from the USB descriptors, in units of 4 kilobits per second. See the USB MIDI 2.0 specification for details |
| `MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits` | The device's maximum output bandwidth from the USB descriptors, in units of 4 kilobits per second. See the USB MIDI 2.0 specification for details |
| `CalculatedMaxDeviceInputBandwidthBitsPerSecond` | `MaxDeviceInputBandwidthIn4KBitsPerSecondUnits`, worked out in bits per second |
| `CalculatedMaxDeviceOutputBandwidthBitsPerSecond` | `MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits`, worked out in bits per second |

## Functions

| Function | Description |
| --------------- | ----------- |
| `IncludesGroup(group)` | Returns true if this block covers the group |
| `AsEquivalentFunctionBlock()` | Returns a `MidiFunctionBlock` that's roughly the same as this block, so your application only has to deal with one kind of block when it shows this information |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ShortLabel` | The short name for a group terminal block in the user's language |
| `ShortLabelPlural` | The plural of the short name |
| `LongLabel` | The full name in the user's language |
| `LongLabelPlural` | The plural of the full name |
