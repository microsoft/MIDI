---
layout: sdk_reference_page
title: MidiFunctionBlock
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
implements: Windows.Foundation.IStringable
description: Represents a MIDI 2.0 Function Block per the specification
---

The first MIDI 2.0 USB specification introduced group terminal blocks. After it was approved, it turned out that group terminal blocks weren't enough, for two main reasons:

1. Group terminal blocks exist only for USB, so other transports, such as network and virtual devices, don't have them.
2. Group terminal blocks are fixed in the USB descriptors, so they can't change while the device is running.

Group terminal blocks are still available, but function blocks are the better way to describe what a device can do. When a device has both, use the function blocks.

A function block is one job a MIDI 2.0 device does. It can cover one or more groups, and unless it's a static function block, those groups can change while the device is running. For example, a device's tone generator might need 64 channels so it can play many sounds at once. One way to do that is a function block that covers 4 groups of 16 channels each (4 x 16 = 64).

Function blocks also tell you which groups an endpoint really uses. If an endpoint has 4 active function blocks that together cover only group indexes 0 to 5, offer only those groups to the people using your application. That's much less cluttered than always showing all 16 groups.

Show a function block's name along with its group numbers. Messages are still sent to the endpoint with a group number in each Universal MIDI Packet, but the function block tells people what that group number is for.

The specification lets function blocks cover more than one group, and overlap each other, so different functions can share a group.

The Windows MIDI Services API uses function blocks in three ways:

1. From `MidiEndpointDeviceInformation.GetDeclaredFunctionBlocks()`, for the function blocks found during endpoint discovery. These are read-only.
2. From `MidiGroupTerminalBlock.AsEquivalentFunctionBlock()`, which turns a group terminal block into a function block for convenience. These are read-only.
3. Created by your application as part of the definition of a virtual device for app-to-app MIDI. You can change these until you add them to the device definition.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiFunctionBlock()` | Creates an empty function block |

## Properties

Most properties match the function block fields in the UMP specification one for one. The API puts the name together for you, and turns values into enums where it can.

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if this function block is read-only. If you set a property on a read-only function block, nothing happens, and no error is raised |
| `Number` | The block's index, from 0 to 31. It's called "number" to match the specification |
| `Name` | The function block's name, put together from the messages that carried it |
| `IsActive` | True if this block is active |
| `Direction` | Which way messages go, from the block's point of view |
| `UIHint` | A hint about how to show this block in a user interface. Use it to decide what to show first, not to hide blocks from people completely |
| `RepresentsMidi10Connection` | Whether this block stands for a MIDI 1.0 connection, and how to treat it. A new block starts as `Not10`, so a virtual device only needs to set this when the block really does connect to MIDI 1.0 |
| `FirstGroup` | The first group this block covers |
| `GroupCount` | How many groups this block covers |
| `MidiCIMessageVersionFormat` | The MIDI-CI message version the block uses |
| `MaxSystemExclusive8Streams` | The most System Exclusive 8 streams the block allows at once. See the UMP specification for how to use this value |

## Functions

| Function | Description |
| --------------- | ----------- |
| `IncludesGroup(group)` | Returns true if this block covers the group |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ShortLabel` | The short name for a function block in the user's language |
| `ShortLabelPlural` | The plural of the short name |
| `LongLabel` | The full name in the user's language |
| `LongLabelPlural` | The plural of the full name |
