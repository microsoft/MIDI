---
layout: sdk_reference_page
title: MidiGroupTerminalBlockDirection
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: The message flow for a group terminal block, from the block's point of view
---

Which way messages go for a group terminal block. As the specification says, this is from the block's point of view. So a group terminal block with `BlockOutput` sends messages, which means your application receives them as input.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Bidirectional` | `0x00000000` | The block both sends and receives messages |
| `BlockInput` | `0x00000001` | The block receives messages |
| `BlockOutput` | `0x00000002` | The block sends messages |
