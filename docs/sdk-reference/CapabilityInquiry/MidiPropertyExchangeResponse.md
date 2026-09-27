---
layout: sdk_reference_page
title: MidiPropertyExchangeResponse
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: The answer to a property exchange request, with all its pieces already put back together
---

A device can say no in two different ways, and it helps to tell them apart. A negative acknowledgment (NAK) means the request itself failed. `Status` is `NegativeAcknowledgment`, and `NakStatusCode` says why. A reply header with a status other than 200 means the device answered but won't give you that resource. `Status` is also `NegativeAcknowledgment`, and `ResourceStatus` holds the number.

## Properties

| Property | Description |
| -------- | ----------- |
| `Status` | How the request ended |
| `ResponderMuid` | Which responder answered |
| `RequestId` | The request this answers |
| `Header` | The reply header. It holds the device's own status for the request, and anything the device wants to say about the data, such as the total count for a list sent in pages. Null when the device sent a header that couldn't be read as JSON |
| `HeaderText` | The header as it arrived. It holds what the device sent even when it couldn't be read as JSON |
| `ResourceStatus` | The status the device put in its reply header. 200 means it answered. Anything else means the device said no, and the header usually says why in a `message` property |
| `ChunkCount` | How many pieces the reply arrived in. It's only useful for seeing how big a transfer was |
| `Body` | The property data, put back together |
| `BodyAsText` | The same data as text. Every resource the MIDI Association defines is JSON text. Empty when the reply header names an encoding this can't decode |
| `BodyAsJson` | The same data, read as JSON. Null when the body isn't JSON, including when it arrived encoded. A list resource reads as an array and everything else as an object, so this returns the type they share |
| `NakStatusCode` | Set when the device answered with a negative acknowledgment instead of a reply |
| `NakStatusMessage` | The device's own explanation. The specification asks that you show it to the person using your application |
