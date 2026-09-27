---
layout: sdk_reference_page
title: MidiStreamConfigRequestReceivedEventArgs
namespace: Windows.Devices.Midi2.Transports.Virtual
type: runtimeclass
description: Arguments supplied when a client of this endpoint has requested stream configuration
---

When a virtual device gets a Stream Configuration Request message, the API reads the message and passes its values to you in this event. Your virtual device should then answer the request, as the UMP protocol negotiation specification describes.

## Properties

| Property | Description |
| --- | --- |
| `Timestamp` | The incoming message's timestamp |
| `PreferredMidiProtocol` | The `MidiProtocol` the client is asking for |
| `RequestEndpointTransmitJitterReductionTimestamps` | Whether the client asked the endpoint to send jitter reduction timestamps. They aren't supported |
| `RequestEndpointReceiveJitterReductionTimestamps` | Whether the client asked the endpoint to receive jitter reduction timestamps. They aren't supported |

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/simple-app-to-app-midi)
* [C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
