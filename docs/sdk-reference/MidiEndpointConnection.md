---
layout: sdk_reference_page
title: MidiEndpointConnection
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable, Windows.Devices.Midi2.IMidiMessageReceivedEventSource, Windows.Devices.Midi2.IMidiEndpointConnectionSource
description: The primary way to send and receive messages with an endpoint.
tags: session, connection, endpoint
---

A `MidiEndpointConnection` is your application's connection to one endpoint. You get one from `MidiSession.CreateEndpointConnection`, and it lasts only as long as that session.

Each connection uses memory for its send and receive buffers, plus a thread to process messages. So a session should usually open only one connection to each endpoint. If you want to split up incoming messages, for example by group or channel, add message processing plugins to that one connection instead of opening more.

The session gives you the connection before it's open. That way you can attach your event handlers and any message processing plugins before the first message arrives. When you're ready, call `Open()`. That connects to the service, sets up the message queues, and starts sending and receiving.

## Properties

| Property | Description |
| -------- | ----------- |
| `ConnectionId` | A GUID that identifies this connection. Pass it to `MidiSession.DisconnectEndpointConnection` to close the connection |
| `ConnectedEndpointDeviceId` | The id of the endpoint this connection is for. It's the same id that enumeration returns |
| `LogMessageDataValidationErrorDetails` | When true, details about messages that fail validation are also written to Event Tracing for Windows (ETW), along with other errors. Useful while debugging |
| `Tag` | Holds any extra information you want to keep with the connection |
| `IsOpen` | True when the connection is open. A new connection stays closed until you call `Open()` |
| `Settings` | The settings used to create this connection. Treat this as read-only |
| `MessageProcessingPlugins` | The message processing plugins attached to this connection. Each one gets a chance to handle incoming messages |

## Static Member Functions

| Static Function | Description |
| -------- | ----------- |
| `GetDeviceSelector()` | Returns the selector string for finding endpoints that work with this API, for when you enumerate devices with `Windows.Devices.Enumeration` yourself |
| `SendMessageSucceeded(sendResult)` | Returns true if the result from a send function means the message was sent |
| `SendMessageFailed(sendResult)` | Returns true if the result from a send function means the send failed |

## Other Functions

| Function | Description |
| -------- | ----------- |
| `Open()` | Opens the connection and starts receiving messages. Returns true if the connection opened. Attach your `MessageReceived` handler before you call this |
| `AddMessageProcessingPlugin(plugin)` | Attaches a message processing plugin to this connection. Returns a [`MidiMessageProcessingPluginAddResult`]({{ site.baseurl }}/sdk-reference/MidiMessageProcessingPluginAddResultEnum/) that says whether it was added. Check it, because a plugin that wasn't added never sees any messages |
| `RemoveMessageProcessingPlugin(id)` | Removes the message processing plugin with this id |
| `GetSupportedMaxMidiWordsPerTransmission()` | Returns the most MIDI words you can send in one call. See [Sending more data than fits in one call](#sending-more-data-than-fits-in-one-call) |

## Sending and receiving messages

There are several ways to send messages. Each one suits different languages and different ways of storing message data.

C++ developers, and developers using other languages that can work with COM and pointers, can also use the [COM extensions]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions) to send and receive messages on an open connection.

### Sending one message

Each of these functions sends one message. The message must be one complete, valid Universal MIDI Packet (UMP).

| Function | Description |
| -------- | ----------- |
| `SendSingleMessagePacket(message)` | Sends any type that implements `IMidiUniversalPacket`, such as `MidiMessage64` or a strongly typed message class |
| `SendSingleMessageStruct(timestamp, wordCount, message)` | Sends a fixed-size `MidiMessageStruct` that holds `wordCount` valid words. Any other words are ignored |
| `SendSingleMessageWordArray(timestamp, startIndex, wordCount, words)` | Sends one message from an array of words. Some languages copy the whole array when they call this, so it may not be the fastest choice for yours |
| `SendSingleMessageWords(timestamp, word0)` | Sends one 32-bit UMP, given as one 32-bit word. This is often the fastest way to send a message of this size |
| `SendSingleMessageWords(timestamp, word0, word1)` | Sends one 64-bit UMP, given as two 32-bit words. This is often the fastest way to send a message of this size |
| `SendSingleMessageWords(timestamp, word0, word1, word2)` | Sends one 96-bit UMP, given as three 32-bit words. This is often the fastest way to send a message of this size |
| `SendSingleMessageWords(timestamp, word0, word1, word2, word3)` | Sends one 128-bit UMP, given as four 32-bit words. This is often the fastest way to send a message of this size |
| `SendSingleMessageBuffer(timestamp, byteOffset, byteCount, buffer)` | Sends one UMP from a buffer of bytes, starting at `byteOffset`. `byteCount` must match the size that the message type in the first 4 bits calls for. The bytes must be in order, starting with the most significant byte of the first word, which is the byte that holds the message type. `IMemoryBuffer` is how WinRT lets you work with a block of memory, so it's the closest you can get to passing a pointer |

> **Tip:** In every function that takes a timestamp, **pass 0 (zero) to skip the scheduler and send the message right away**. `MidiClock::TimestampConstantSendImmediately` is the same value. Any other timestamp is the exact time the service should send the message. The service's scheduler slows down when it holds thousands of messages, so don't schedule too many at once, or too far ahead.

### Sending several messages in one call

When you send several messages in one call, every message must be complete. Never split one UMP across two calls. If a call holds anything other than whole, valid UMPs, it fails.

Before you build the buffer or list, call `GetSupportedMaxMidiWordsPerTransmission` to find the most 32-bit MIDI words one call can carry. The number can change, so don't hard-code it.

These functions send everything at once, without making an extra copy. Every message gets the same timestamp.

| Function | Description |
| -------- | ----------- |
| `SendMultipleMessagesBuffer(timestamp, byteOffset, byteCount, buffer)` | Sends the messages in an `IMemoryBuffer`, starting at `byteOffset` and continuing for `byteCount` bytes |
| `SendMultipleMessagesWordArray(timestamp, startIndex, wordCount, words)` | Sends the messages in an array, starting at the zero-based `startIndex` and continuing for `wordCount` words. Every message in that range must be whole and valid |

These functions copy the data into a new buffer first, and then send it in one call. Every message gets the same timestamp.

| Function | Description |
| -------- | ----------- |
| `SendMultipleMessagesWordList(timestamp, words)` | Sends the messages in an `IIterable` of 32-bit unsigned integers. Put each message's words in order, one message after another, and make sure each message has the right number of words for its message type |
| `SendMultipleMessagesStructList(timestamp, messages)` | Sends an `IIterable` of `MidiMessageStruct` messages |
| `SendMultipleMessagesStructArray(timestamp, startIndex, messageCount, messages)` | Sends an array of `MidiMessageStruct` messages, starting at `startIndex` and continuing for `messageCount` messages |

This function sends each packet separately, because each one has its own timestamp.

| Function | Description |
| -------- | ----------- |
| `SendMultipleMessagesPacketList(messages)` | Sends an `IIterable` of `IMidiUniversalPacket` messages, each with its own timestamp |

> **Tip:** To learn how WinRT collections work, and how they convert to types like `std::vector`, see [Collections with C++/WinRT](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/collections).

### Sending more data than fits in one call

If a call holds more words than `GetSupportedMaxMidiWordsPerTransmission` allows, the whole call is rejected and **nothing is sent**. The result is `MidiSendMessageResults.Failed` combined with `MidiSendMessageResults.TransmissionWordCountExceeded`. Because nothing reached the device, it's safe to try again with a smaller buffer.

Some data is always too big for one call. A System Exclusive 7 packet carries six data bytes in each 64-bit UMP, so a 64 KB bulk dump becomes about 10,900 UMPs, or about 21,800 MIDI words. That will never fit in one call, so your code has to split it up.

When you split a large buffer:

1. Call `GetSupportedMaxMidiWordsPerTransmission` on the same connection you're sending to. Don't hard-code the value, and don't assume it's the same for every endpoint or every release.
2. Split only between messages. One UMP must never be spread across two calls.
3. Stop as soon as a call fails. Sending the rest after a failure only makes it harder for the device to recover.
4. Decide ahead of time what to do if only part of the data gets through. The chunks that were accepted have already reached the device. For System Exclusive in particular, a message that was only partly delivered usually means you have to give up and start the whole transfer again.

```cpp
// Send a large buffer as a series of transmissions, without splitting any UMP.
const uint32_t maxWords = connection.GetSupportedMaxMidiWordsPerTransmission();

uint32_t offset{ 0 };

while (offset < totalWords)
{
    // Gather whole messages until the next one would not fit
    uint32_t chunkWords{ 0 };

    while (offset + chunkWords < totalWords)
    {
        auto packetType = MidiMessageHelper::GetPacketTypeFromMessageFirstWord(words[offset + chunkWords]);

        if (packetType == MidiPacketType::UnknownOrInvalid) return;

        // the MidiPacketType value is also the message length in MIDI words
        const uint32_t messageWords = static_cast<uint32_t>(packetType);

        if (chunkWords + messageWords > maxWords) break;

        chunkWords += messageWords;
    }

    if (chunkWords == 0) return;

    auto sendResult = connection.SendMultipleMessagesWordArray(
        MidiClock::TimestampConstantSendImmediately(), offset, chunkWords, words);

    if (MidiEndpointConnection::SendMessageFailed(sendResult))
    {
        // Any earlier chunks have already reached the device. Do not send the rest.
        break;
    }

    offset += chunkWords;
}
```

> **Note for framework and language projection authors:** If you wrap this API for another language or app framework, do this splitting inside your wrapper. Don't make the applications built on your wrapper do it. They usually can't reach `GetSupportedMaxMidiWordsPerTransmission` through your wrapper, so they would have to hard-code a limit, and a hard-coded limit isn't guaranteed to stay correct.


## Events

The connection waits for your `MessageReceived` handler to finish before it hands over the next message, so keep your handler fast.

Applications are usually much faster than devices. But if your handler can't keep up, the incoming message queue can fill up and cause errors. MIDI 2.0 has no speed limit for devices, and USB 3 and network devices, among others, can send a lot of messages in a very short time.

If you need to do slow work with incoming messages, copy them to your own queue and process them on another thread.

| Event | Description |
| -------- | ----------- |
| `MessageReceived(source, args)` | From `IMidiMessageReceivedEventSource`. This is how you receive MIDI messages, one at a time |
| `EndpointDeviceDisconnected(source, args)` | From `IMidiEndpointConnectionSource`. Raised when the endpoint device disconnects |
| `EndpointDeviceReconnected(source, args)` | From `IMidiEndpointConnectionSource`. Raised when the endpoint device reconnects, if `AutoReconnect` is turned on in the connection's [settings]({{ site.baseurl }}/sdk-reference/MidiEndpointConnectionSettings) |

> **Note:** Attach your event handlers and add any message processing plugins before you call `Open()`.

## Samples

The "API client basics" samples send and receive messages using the two built-in diagnostic loopback endpoints. To learn more about those endpoints, see [Diagnostic endpoints]({{ site.baseurl }}/kb/diagnostic-endpoints/).

* [C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/basics/main_client_basics.cpp)
* [C# Sample](https://github.com/microsoft/MIDI/blob/main/samples/csharp-net/basics/Program.cs)
