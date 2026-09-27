---
layout: sdk_reference_page
title: MidiSystemExclusiveReceiver
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
type: runtimeclass
implements: Windows.Foundation.IClosable
description: Receives SysEx 7 data from an endpoint connection and raises byte events
---

Use this type to collect incoming SysEx 7 messages from a `MidiEndpointConnection`, and get them as blocks of bytes.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiSystemExclusiveReceiver(sourceConnection, sourceGroup, maximumBytesPerEvent)` | Creates a receiver for one connection and group. `maximumBytesPerEvent` is the most bytes it collects before it raises an event |

## Events

| Event | Description |
| ----- | ----------- |
| `BytesReceived` | Raised when a complete SysEx message is ready, or when the collected bytes reach `maximumBytesPerEvent` |

## Methods

| Method | Description |
| ------ | ----------- |
| `Start()` | Starts receiving. Returns true if it worked |
| `Stop()` | Stops receiving, and passes on any bytes it's holding. That may raise more `BytesReceived` events before it returns |

## Properties

| Property | Description |
| -------- | ----------- |
| `IsReceiving` | True while it's receiving |
| `CountBytesReceived` | The total number of bytes received |
| `CountMessagesReceived` | The total number of complete SysEx messages received |

## Remarks

`maximumBytesPerEvent` trades memory for how often events are raised. A smaller value means more frequent events. A larger value means fewer events, but more memory held between them.

## Samples

These samples receive a System Exclusive message and write it to a `.syx` file. They replace the WinMM work of handling `MIM_LONGDATA`, preparing and queuing `MIDIHDR` buffers again and again, and putting back together a message that arrived across several of them.

* [C++/WinRT sysex-file-receiver](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/sysex-file-receiver)
* [C# sysex-file-receiver](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/sysex-file-receiver)
