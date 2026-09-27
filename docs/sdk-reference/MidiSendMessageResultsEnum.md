---
layout: sdk_reference_page
title: MidiSendMessageResults
namespace: Windows.Devices.Midi2
type: enum
description: Return value when sending a MIDI message
---

Every function that sends messages returns a `MidiSendMessageResults` value. Check it to make sure the message was sent. The value is a set of flags combined with a bitwise OR: one flag says whether the send worked, and if it didn't, other flags say why.

`MidiEndpointConnection` has two static functions, `SendMessageSucceeded` and `SendMessageFailed`, that read the result for you. If the send failed, you can then check the other flags to learn the reason.

## Properties

These values are flags, and they can be combined.

| Property | Value | Description |
| -------- | ----- | ----------- |
| `Succeeded` | `0x80000000` | The send worked. |
| `Failed` | `0x10000000` | The send failed. One or more of the flags below says why. |
| `BufferFull` | `0x00010000` | The outgoing buffer to the service was full, so the message couldn't be sent. |
| `EndpointConnectionClosedOrInvalid` | `0x00040000` | The connection was closed, or stopped being valid, before the message could be sent. |
| `InvalidMessageTypeForWordCount` | `0x00100000` | The number of words sent doesn't match the message type in the first word. |
| `InvalidMessageOther` | `0x00200000` | The message wasn't valid for some other reason. |
| `DataIndexOutOfRange` | `0x00400000` | Reading the whole message would go past the end of the array, collection, or buffer you passed. |
| `TimestampOutOfRange` | `0x00800000` | The timestamp is too far in the future to schedule. |
| `TransmissionWordCountExceeded` | `0x01000000` | The call held more MIDI words than one call can carry. See `GetSupportedMaxMidiWordsPerTransmission` on `MidiEndpointConnection`. |

## Example

```cpp
auto sendResult = myConnection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), 0x28675309);

if (MidiEndpointConnection::SendMessageSucceeded(sendResult))
{
    // do something in the case of success
}
else
{
    // one or more failure reasons are in the result. Use the bitwise AND (&) operator to check each one.
}

```
