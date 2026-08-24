---
layout: sdk_reference_page
title: MidiSystemExclusiveReceivedEventArgs
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
type: runtimeclass
description: Event data produced when SysEx data is received by MidiSystemExclusiveReceiver
---

Provided in `MidiSystemExclusiveReceiver.BytesReceived`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Group` | The source `MidiGroup` |
| `Bytes` | Received bytes, including `0xF0` and `0xF7` framing bytes |
| `IsPartial` | True when the block does not contain a complete matched `F0`/`F7` message pair |

## Remarks

A single event may contain one complete message, multiple complete messages, or a partial fragment, depending on buffering and pacing.
