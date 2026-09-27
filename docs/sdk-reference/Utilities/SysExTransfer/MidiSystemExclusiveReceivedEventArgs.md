---
layout: sdk_reference_page
title: MidiSystemExclusiveReceivedEventArgs
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
type: runtimeclass
description: Event data produced when SysEx data is received by MidiSystemExclusiveReceiver
---

Comes with the `MidiSystemExclusiveReceiver.BytesReceived` event.

## Properties

| Property | Description |
| -------- | ----------- |
| `Group` | The `MidiGroup` the data came from |
| `Bytes` | The bytes received, including the `0xF0` and `0xF7` bytes that start and end each message |
| `IsPartial` | True when the block doesn't hold a complete message, from `F0` to `F7` |

## Remarks

One event may hold one complete message, several complete messages, or part of a message. It depends on how the data was buffered and how fast it arrived.
