---
layout: sdk_reference_page
title: MidiPropertyExchangeResponse
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: The answer to a property exchange request, with every chunk already put back together
---

A device can decline in two different ways, and they are worth telling apart. A negative acknowledgment means the transaction itself failed; `Status` is `NegativeAcknowledgment` and `NakStatusCode` says why. A reply header carrying a status other than 200 means the device answered but would not give you that resource; `Status` is also `NegativeAcknowledgment`, and `ResourceStatus` holds the number.

## Properties

| Property | Description |
| -------- | ----------- |
| `Status` | How the request ended |
| `ResponderMuid` | Which responder answered |
| `RequestId` | The request this answers |
| `Header` | The reply header, which carries the device's own status for the request and anything it wants to say about the data, for example the total count behind a paged list. Null when the device sent a header this could not read as JSON |
| `HeaderText` | The header as it arrived, which still holds what came even when it would not parse |
| `ResourceStatus` | The status the device put in its reply header. 200 means it answered; anything else is the device declining, and the header usually says why in a `message` property |
| `ChunkCount` | How many chunks the reply arrived in. Worth knowing only to understand a transfer's cost |
| `Body` | The property data, reassembled |
| `BodyAsText` | The same data as text. Every resource defined by the MIDI Association is JSON text. Empty when the reply header declares an encoding this does not decode |
| `BodyAsJson` | The same data parsed. Null when the body is not JSON, which includes the case where it arrived encoded. A list resource parses as an array and everything else as an object, so this is the common interface rather than either one |
| `NakStatusCode` | Set when the device answered with a negative acknowledgment rather than a reply |
| `NakStatusMessage` | The device's own explanation. The specification asks that it be shown to the person using the application |
