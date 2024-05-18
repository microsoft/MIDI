---
layout: api_page
title: MidiFunctionBlock
parent: Metadata
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiFunctionBlock

The original MIDI 2.0 USB specification includes the concept of a Group Terminal Block. After ratification of that specification, it was found that Group Terminal Blocks were insufficient for two main reasons:
1. Group Terminal Blocks are USB-specific, and so are not available on other transports like Network or Virtual.
2. Group Terminal Blocks are static, defined in USB descriptors, and so cannot change during runtime.

Group Terminal Blocks are still available, but function blocks are the preferred approach for defining capapbilities of a device. When both are available, you should use the function block information.

A function block represents a function of a MIDI 2.0 device. A function block may span one or more groups, and if not a static function block, those group numbers may change during operation. For example, a Tone Generator function of a device may need 64 channels to represent its multi-timbral nature. One way it can accomplish this is to declare a function block which spans 4 groups, each of which has 16 channels of data (16x4 = 64).

Function blocks also represent the valid groups for communication with an endpoint. If an endpoint declares 4 function blocks, which together cover only group indexes 0-5, and all of those blocks are marked active, only those groups should be available to users of an application. This helps cut down on clutter caused by always displaying 16 groups.

Function blocks have names which should be displayed to the user along with the group numbers. In the end, the actual addressible entity is the endpoint stream with the group number in the Universal MIDI Packet. But the function block provides context for that group number.

Per the specification, function blocks can span more than one group, and can overlap with each other so that different functions can be available on the same group.

Function blocks are used in the Windwos MIDI Services API in three ways:

1. A property of a MidiEndpointDeviceInformation object, representing function blocks discovered through endpoint discovery. These function blocks are read-only.
2. The return value of the AsEquivalentFunctionBlock method of the GroupTerminalBlock class. This is a convenience function. These function blocks are read-only
3. Provided by the application as part of the device definition for a virtual device in app-to-app MIDI. These function blocks are editable before adding them to the device definition.

## Properties

Most properties are 1:1 with the MIDI 2.0 UMP specification section on function blocks. We assemble the name for you and map values to enumerations when possible.

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if this function block should be treated as read-only. If you attempt to assign a value to a property in a read-only function block, the assignment will silently fail. |
| `Number` | The index of the block 0-31. We use "number" here to be consistent with the specification |
| `Name` | The assembled name of the function block |
| `IsActive` | True if this block is active |
| `Direction` | The direction of the block from the block's point of view. |
| `UIHint` | A hint which tells you how this block should be treated in a user interface. This should be considered a "soft filter" for display, not a mechanism to keep blocks completely hidden from a user. |
| `Midi10Connection` | How to treat this block if it is a MIDI 1.0 connection |
| `FirstGroupIndex` | Zero-based index of the first group spanned by this block. |
| `GroupCount` | The number of groups spanned. |
| `MidiCIMessageVersionFormat` | MIDI CI version format value |
| `MaxSystemExclusive8Streams` | The maximum number of System Exclusive 8 streams allowed. Please refer to the UMP specification for how to treat this value. |

## Functions

| Function | Description |
| --------------- | ----------- |
| `MidiFunctionBlock()` | Construct an empty function block |
| `IncludesGroup(group)` | Helper function which returns true if this function exists on the supplied group |

## IDL

[MidiFunctionBlock IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiFunctionBlock.idl)
