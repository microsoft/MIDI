---
layout: sdk_reference_page
title: MidiEndpointConnection
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IStringable, Windows.Devices.Midi2.IMidiMessageReceivedEventSource, Windows.Devices.Midi2.IMidiEndpointConnectionSource
description: The primary way to send and receive messages with an endpoint.
tags: session, connection, endpoint
---

The `MidiEndpointConnection` type represents a single connection to a single endpoint managed by Windows MIDI Services. It is created using the functions of the `MidiSession`, and is tied to the lifetime of that session.

Connections allocate resources including send/receive buffers, and processing threads. For that reason, a session should generally not open more than one connection to a single endpoint. If you need to partition out messages more easily (by group or channel, for example) the `MessageProcessingPlugins` collection will help you do that.

To ensure an application is able to wire up processing plugins and event handlers before the connection is active, the connection returned by the `MidiSession` is not yet open. Once the connection is acquired, the application should assign event handlers, and optionally assign any message processing plugins. Once complete, the application calls the `Open()` function to connect to the service, create the queues, and begin sending and receiving messages.

## Properties

| Property | Description |
| -------- | ----------- |
| `ConnectionId` | The generated GUID which uniquely identifes this connection instance. This is what is provided to the `MidiSession` when disconnecting an endpoint |
| `ConnectedEndpointDeviceId` | The system-wide identifier for the endpoint device. This is returned through enumeration calls. |
| `LogMessageDataValidationErrorDetails` | When true, message data validation errors are logged to any connected ETL listener, in addition to other errors. |
| `Tag` | You may use this `Tag` property to hold any additional information you wish to have associated with the connection. |
| `IsOpen` | True if this connection is currently open. When first created, the connection is not open until the consuming code calls the `Open` method |
| `Settings` | Settings used to create this connection. Treat this as read-only. |
| `MessageProcessingPlugins` | Collection of all message processing plugins which will optionally handle incoming messages. |

## Static Member Functions

| Static Function | Description |
| -------- | ----------- |
| `GetDeviceSelector ()` | Returns the device selector used for enumerating endpoint devices compatible with this API. |
| `SendMessageSucceeded (sendResult)` | Helper function to decipher the return result of a message sending function to tell if it succeeded. |
| `SendMessageFailed (sendResult)` | Helper function to decipher the return result of a message sending function to tell if it failed. |

## Other Functions

| Function | Description |
| -------- | ----------- |
| `Open()` | Open the connection and start receiving messages. Wire up the message event handler before calling this method. |
| `AddMessageProcessingPlugin (plugin)` | Add a message processing plugin to this connection |
| `RemoveMessageProcessingPlugin (id)` | Remove a message processing plugin from this connection |
| `GetSupportedMaxMidiWordsPerTransmission` | Returns the maximum number of MIDI words which can be sent in a single call |

# Sending and Receiving Messages through WinRT

There are multiple mechanisms available for sending and recieving messages, each suitable to different programming languages and app data storage models.

In addition to these functions, C++ (and other COM-aware and pointer-friendly languages) developers can optionally use the COM Extensions to send and receive messages on a valid and open connection.

## Single-Message Sender Functions

Each function sends a single message at a time. Each message must be a complete and valid Universal MIDI Packet.

| Function | Description |
| -------- | ----------- |
| `SendSingleMessagePacket (message)` | Send an `IMidiUniversalPacket`-implementing type such as `MidiMessage64` or a strongly-typed message class. |
| `SendSingleMessageStruct  (timestamp, message, wordCount)` | Send a fixed-sized `MidiMessageStruct` containing `wordCount` valid words. Additional words are ignored. |
| `SendSingleMessageWordArray (timestamp, startIndex, wordCount, words)` | Send an array of words for a single message. Note: Some projections will send the entire array as a copy, so this may not be the most effecient way to send messages from your language. |
| `SendSingleMessageWords (timestamp, word0)` | Send a single 32-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| `SendSingleMessageWords (timestamp, word0, word1)` | Send a single 64-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| `SendSingleMessageWords (timestamp, word0, word1, word2)` | Send a single 96-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| `SendSingleMessageWords (timestamp, word0, word1, word2, word3)` | Send a single 128-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| `SendSingleMessageBuffer (timestamp, byteOffset, byteCount, buffer)` | Send a single Universal MIDI Packet as bytes from a buffer. The number of bytes sent must match the size read from the first 4 bits of the data starting at the specified offset, and must be laid out correctly with the first byte corresponding to the MSB of the first word of the UMP (the word which contains hte message type). If you want to manage a chunk of buffer memory, the `IMemoryBuffer` type is the acceptable WinRT approach, and is as close as you get to sending a pointer into a buffer. |

> # Tip: 
> In all the functions which accept a timestamp to schedule the message, **you can send a timestamp of 0 (zero) to bypass the scheduler and send the message immediately** or use the `MidiClock::TimestampConstantSendImmediately` static property. Otherwise, the provided timestamp is treated as an absolute time for when the message should be sent from the service. Note that the service-based scheduler (currently based on a `std::priority_queue`) gets less efficient when there are thousands of messages in it, so it's recommended that you not schedule too many messages at a time or too far out into the future. 

## Multiple-Message Sender Functions

When sending multiple messages, each message must be complete within the transmission. A single UMP shall not be split across multiple transmissions. If the transmission does not contain whole valid UMPs, the call will fail.

When constructing the buffer or list to send, be sure to call `GetSupportedMaxMidiWordsPerTransmission` to get the maximum number of 32-bit MIDI words which can be sent in a single transmission. This number may change over time, and so should not be assumed to be a static constant.

These functions send all data at once, without allocating any additional buffers. Each message is sent with the same timestamp.

| Function | Description |
| -------- | ----------- |
| `SendMultipleMessagesBuffer (timestamp, byteOffset, byteCount, buffer)` | Send multiple messages using the `IMemoryBuffer` approach and a single timestamp. |
| `SendMultipleMessagesWordArray (timestamp, startIndex, wordCount, words)` | Similar to the WordList approach, this will send multiple messages from an array, starting at the zero-based `startIndex` and continuing for `wordCount` words. The messages within that range must be valid and complete.|

These functions need to copy the data to a new buffer, and then send in a single call. Each message is sent with the same timestamp.

| Function | Description |
| -------- | ----------- |
| `SendMultipleMessagesWordList (timestamp, words)` | When supplied an `IIterable` of 32 bit unsigned integers, this sends more than one message with the same timestamp. Message words must be ordered contiguously from word-0 to word-n for each message, and the message types must be valid for the number of words for each message.|
| `SendMultipleMessagesStructList (timestamp, messages)` | Send an `IIterable` of `MidiMessageStruct` messages. All messages are sent with the same timestamp|
| `SendMultipleMessagesStructArray (timestamp, startIndex, messageCount, messages)` | Send an an array of `MidiMessageStruct` messages, starting at `startIndex` and continuing for `messageCount` messages.|

This function sends each packet one at a time, because each packet has its own timestamp

| Function | Description |
| -------- | ----------- |
| `SendMultipleMessagesPacketList (messages)` | Send an `IIterable` of `IMidiUniversalPacket` messages, each with their own timestamp. |

> # Tip 
> To learn more about how collections are handled in WinRT, and how they may convert to constructs like `std::vector`, see the [Collections with C++/WinRT](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/collections) page in our documentation.


## Events

The `MessageReceived` event is raised synchronously, and needs to be handled quickly and efficiently by the calling application.

Applications are typically much faster than devices at handling messages. However, failing to drain the incoming message queue fast enough can result in transmission errors. With MIDI 2.0 there is no upper performance limit on devices, and USB 3 and Network MIDI devices, among others, are capable of transmitting a large number of messages in a very short period of time.

If you need to do long-running processing of incoming messages, add them to your own incoming queue and have them processed by another application thread.

| Event | Description |
| -------- | ----------- |
| `MessageReceived (source, args)` | From `IMidiMessageReceivedEventSource`. This is the event for receiving MIDI Messages, one at a time. |

> # Note: 
> Wire up event handlers and add message processing plugins prior to calling `Open()`. 

## Sample

Here's an excerpt from the full "API client basics" sample. It shows sending and receiving messages using the two built-in loopback endpoints. For more information on the loopback endpoints, see [diagnostics endpoints](../../endpoints/diagnostic-endpoints.md).

Complete examples [available on Github](https://aka.ms/midirepo)
