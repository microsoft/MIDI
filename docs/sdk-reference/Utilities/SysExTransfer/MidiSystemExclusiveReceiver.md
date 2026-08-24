---
layout: sdk_reference_page
title: MidiSystemExclusiveReceiver
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
type: runtimeclass
implements: Windows.Foundation.IClosable
description: Receives SysEx 7 data from an endpoint connection and raises byte events
---

Use this type to assemble incoming SysEx 7 traffic from a `MidiEndpointConnection` and receive it as byte blocks.

## Constructor

| Constructor | Description |
| ----------- | ----------- |
| `MidiSystemExclusiveReceiver(sourceConnection, sourceGroup, maximumBytesPerEvent)` | Creates a receiver bound to one connection and group. `maximumBytesPerEvent` bounds buffering before events are raised |

## Event

| Event | Description |
| ----- | ----------- |
| `BytesReceived` | Raised when a complete SysEx message is available, or when the buffered byte count reaches `maximumBytesPerEvent` |

## Methods

| Method | Description |
| ------ | ----------- |
| `Start()` | Starts receiving and returns true on success |
| `Stop()` | Stops receiving and flushes any buffered bytes (may raise additional `BytesReceived` events before returning) |

## Properties

| Property | Description |
| -------- | ----------- |
| `IsReceiving` | True while actively receiving |
| `CountBytesReceived` | Total count of bytes received |
| `CountMessagesReceived` | Total count of completed SysEx messages received |

## Remarks

`maximumBytesPerEvent` controls the memory/per-event tradeoff. Smaller values produce more frequent events; larger values reduce event frequency.
