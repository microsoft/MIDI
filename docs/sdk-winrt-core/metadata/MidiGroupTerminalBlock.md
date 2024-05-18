---
layout: api_page
title: MidiGroupTerminalBlock
parent: Metadata
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiGroupTerminalBlock

A Group Terminal Block is a USB-only feature used to describe the groups on a device. When available, Function Blocks are the preferred mechanism for finding active groups, names, and more, meaning that the Group Terminal Block can typically be ignored in those cases. For more context, please see the documentation for the [MidiFunctionBlock](../MidiFunctionBlock/) type.

> Note: In Windows MIDI Services, we translate MIDI 1.0 device "ports" into individual Group Terminal Blocks. Each virtual cable number in the stream, which used to become a separate input or output port, now maps to a group number. For example, a 5 port MIDI 1.0 device will now show up as a single endpoint with 5 Group Terminal Blocks each spanning a single group. 

## Properties

| Property | Description |
| --------------- | ----------- |
| `Number` | Block number |
| `Name` | Name provided by USB. In the case of MIDI 1.0 devices, when available, this is the `iJack` string |
| `Direction` | Direction of the block, from the block's point of view |
| `Protocol` | Information about the protocol in use. Note that the Jitter Reduction values here should be ignored. Jitter reduction timestamp handling is negotiated through protocol negotiation, and is entirely handled by the service |
| `FirstGroupIndex` | The index of the first group spanned by this block |
| `GroupCount` | The number of groups spanned |
| `MaxDeviceInputBandwidthIn4KBitsPerSecondUnits` | Please see the USB MIDI 2.0 specification for the actual value for this field. |
| `MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits` | Please see the USB MIDI 2.0 specification for the actual value for this field. |
| `CalculatedMaxDeviceInputBandwidthBitsPerSecond` | Bits-per-second calculated value for the `MaxDeviceInputBandwidthIn4KBitsPerSecondUnits` property |
| `CalculatedMaxDeviceOutputBandwidthBitsPerSecond` | Bits-per-second calculated value for the `MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits` property |

## Functions

| Function | Description |
| --------------- | ----------- |
| `IncludesGroup(group)` | Helper function which returns true if this function exists on the supplied group |
| `AsEquivalentFunctionBlock()` | Helper function which returns a `MidiFunctionBlock` that is approximately equivalent to this `MidiGroupTerminalBlock`. This is to enable applications to be able to deal with only a single type of block when showing the metadata |

## IDL

[MidiGroupTerminalBlock IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiGroupTerminalBlock.idl)
