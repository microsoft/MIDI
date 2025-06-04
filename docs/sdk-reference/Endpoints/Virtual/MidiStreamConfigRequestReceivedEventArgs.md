---
layout: sdk_reference_page
title: MidiStreamConfigRequestReceivedEventArgs
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Virtual
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Arguments supplied when a client of this endpoint has requested stream configuration
---

When a MIDI Virtual Device receives a request message for stream configuration, the message is parsed and then raised as discrete properties in an event. The virtual device should then respond to the request per the UMP Protocol Negotiation specifications.

## Properties

| Property | Description |
| --- | --- |
| `Timestamp` | Incoming message timestamp |
| `PreferredMidiProtocol` | The `MidiProtocol` being requested by the client |
| `RequestEndpointTransmitJitterReductionTimestamps` | Jitter reduction timestamps are not supported |
| `RequestEndpointReceiveJitterReductionTimestamps` | Jitter reduction timestamps are not supported |

## Examples

[C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/simple-app-to-app-midi/main.cpp)
[C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
