---
layout: sdk_reference_page
title: MidiGroupTerminalBlockDirection
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: The message flow for a group terminal block, from the block's point of view
---

Indicates the message flow for a group terminal block. Note that this is, per the specification, from the group terminal block's point of view. So, for example, a group terminal block specifying `BlockOutput` would be a sender of messages, and therefore used as an input in the API.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Bidirectional` | `0x0` | This block represents a bidirectional function. |
| `BlockInput` | `0x1` | This block represents an input function, from the block's point of view. |
| `BlockOutput` | `0x2` | This block represents an output function, from the block's point of view. |
