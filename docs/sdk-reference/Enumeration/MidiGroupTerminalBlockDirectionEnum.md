---
layout: sdk_reference_page
title: MidiGroupTerminalBlockDirection
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: The message flow for a group terminal block, from the block's point of view
---

Indicates the message flow for a group terminal block. Note that this is, per the specification, from the group terminal block's point of view. So, for example, a group terminal block specifying `BlockOutput` would be a sender of messages, and therefore used as an input in the API.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Bidirectional` | `0x00000000` | This block represents a bidirectional function. |
| `BlockInput` | `0x00000001` | This block represents an input function, from the block's point of view. |
| `BlockOutput` | `0x00000002` | This block represents an output function, from the block's point of view. |
