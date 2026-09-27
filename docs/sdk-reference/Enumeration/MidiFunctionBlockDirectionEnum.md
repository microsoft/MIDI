---
layout: sdk_reference_page
title: MidiFunctionBlockDirection
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: The flow direction for a function block, from the block's point of view
---

Which way messages go for a function block. As the specification says, this is from the function block's point of view. So a function block with `BlockOutput` sends messages, which means your application receives them as input.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Undefined` | `0x00000000` | Not known or not set |
| `BlockInput` | `0x00000001` | The block receives messages |
| `BlockOutput` | `0x00000002` | The block sends messages |
| `Bidirectional` | `0x00000003` | The block both sends and receives messages |
