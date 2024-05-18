---
layout: api_page
title: MidiSendMessageResult
parent: Connections
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiSendMessageResult

When an application sends a message, it should check the result of sending to ensure that the message was transmitted. Each of the message sending functions returns a `MidiSendMessageResult` flags enum. Values in this enum are OR'd together to indicate success or failure, and in the case of failure, the reason.

The `MidiEndpointConnection` type includes static helper functions to process the `MidiSendMessageResult` and determine success or failure. The application may then optionally look at the remaining data to see which failure reason(s) apply. 

```cpp
auto sendResult = myConnection.SendMessageWords(MidiClock::TimestampConstantSendImmediately(), 0x28675309);

if (MidiEndpointConnection::SendMessageSucceeded(sendResult))
{
    // do something in the case of success
}
else
{
    // one or more failure reasons in the result. Use bitwise AND `&` operator to decipher.
}

```

## Properties

| Property | Value | Description |
| -------- | ----- | ----------- |
| `Succeeded` | `0x80000000` | Indicates success. |
| `Failed` | `0x10000000` | Indicates failure. The actual failure reason will be combined with the result. |
| `BufferFull` | `0x00010000` | The message could not be sent because the outgoing buffer to the service was full |
| `EndpointConnectionClosedOrInvalid` | `0x00040000` | The endpoint connection was closed or invalidated before the message could be sent. |
| `InvalidMessageTypeForWordCount` | `0x00100000` | The number of words sent does not match the message type of the first word. |
| `InvalidMessageOther` | `0x00200000` | The message sent was invalid for another reason. |
| `DataIndexOutOfRange` | `0x00400000` | Reading a full message would result in overrunning the provided array, collection, or buffer. |
| `TimestampOutOfRange` | `0x00800000` | The provided timestamp is too far in the future to be scheduled. |
| `MessageListPartiallyProcessed` | `0x00A00000` | The message list was only partially processed. Not all messages were sent. |

## IDL

[MidiSendMessageResult IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiSendMessageResult.idl)

