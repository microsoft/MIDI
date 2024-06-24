---
layout: api_page
title: MidiEndpointConnection
parent: Connections
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointConnection

The `MidiEndpointConnection` type represents a single connection to a single endpoint managed by Windows MIDI Services. It is created using the functions of the `MidiSession`, and is tied to the lifetime of that session.

Connections allocate resources including send/receive buffers, and processing threads. For that reason, a session should generally not open more than one connection to a single endpoint. If you need to partition out messages more easily (by group or channel, for example) the `MessageProcessingPlugins` collection will help you do that.

To ensure an application is able to wire up processing plugins and event handlers before the connection is active, the connection returned by the `MidiSession` is not yet open. Once the connection is acquired, the application should assign event handlers, and optionally assign any message processing plugins. Once complete, the application calls the `Open()` function to connect to the service, create the queues, and begin sending and receiving messages.

## A note on sending messages

All `SendMessageXX` functions send a single Universal MIDI Packet message at a time. The pluralized versions `SendMessagesXX` will send multiple packets, in order, with the same timestamp.

Currently, in the implementation behind the scenes, the service receives each timestamped message one at a time. We have the functions for sending more than one message as a developer convenience for similarity with other platforms, and also to allow for possible future optimization in the service communication code.

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
| `IsAutoReconnectEnabled` | True if endpoints are automatically reconnected if they are, for example, unplugged and then replugged. The value is set at the session level. |

## Static Member Functions

| Static Function | Description |
| -------- | ----------- |
| `GetDeviceSelector()` | Returns the device selector used for enumerating endpoint devices compatible with this API. |
| `SendMessageSucceeded(sendResult)` | Helper function to decipher the return result of a message sending function to tell if it succeeded. |
| `SendMessageFailed(sendResult)` | Helper function to decipher the return result of a message sending function to tell if it failed. |

## Single-Message Sender Functions

| Function | Description |
| -------- | ----------- |
| `SendSingleMessagePacket(message)` | Send an `IMidiUniversalPacket`-implementing type such as `MidiMessage64` or a strongly-typed message class. |
| `SendSingleMessageStruct(timestamp, message, wordCount)` | Send a fixed-sized `MidiMessageStruct` containing `wordCount` valid words. Additional words are ignored. |
| `SendSingleMessageWordArray(timestamp, startIndex, wordCount, words)` | Send an array of words for a single message. Note: Some projections will send the entire array as a copy, so this may not be the most effecient way to send messages from your language. |
| `SendSingleMessageWords(timestamp, word0)` | Send a single 32-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| `SendSingleMessageWords(timestamp, word0, word1)` | Send a single 64-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| `SendSingleMessageWords(timestamp, word0, word1, word2)` | Send a single 96-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| `SendSingleMessageWords(timestamp, word0, word1, word2, word3)` | Send a single 128-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| `SendSingleMessageBuffer(timestamp, byteOffset, byteCount, buffer)` | Send a single Universal MIDI Packet as bytes from a buffer. The number of bytes sent must match the size read from the first 4 bits of the data starting at the specified offset, and must be laid out correctly with the first byte corresponding to the MSB of the first word of the UMP (the word which contains hte message type). If you want to manage a chunk of buffer memory, the `IMemoryBuffer` type is the acceptable WinRT approach, and is as close as you get to sending a pointer into a buffer. |

> Tip: In all the functions which accept a timestamp to schedule the message, **you can send a timestamp of 0 (zero) to bypass the scheduler and send the message immediately** or use the `MidiClock::TimestampConstantSendImmediately` static property. Otherwise, the provided timestamp is treated as an absolute time for when the message should be sent from the service. Note that the service-based scheduler (currently based on a `std::priority_queue`) gets less efficient when there are thousands of messages in it, so it's recommended that you not schedule too many messages at a time or too far out into the future. 

## Multiple-Message Sender Functions

Currently, the service accepts a single message at a time, so these iterate over the messages internally, using the UMP type in the case of words, or just the packets themselves in the case of packets. However, these are present to enable us to optimize sending multiple messages to the service in the future, without you needing to change any of your code.

In methods where there's a single timestamp, messages are sent as quickly as possible on that timestamp. If you send many messages, some other MIDI 1.0 devices may have buffer overflows on the devices themselves. This is not a condition we can detect. In those cases, we recommend using the single message sending functions with a delay between each message.

Finally, there's a practical limit to how many messages can be scheduled ahead of time. Performance degrades as the priority queue size increases, so do not send thousands of messages scheduled at a future time. Sending thousands of messages to be sent immediately is perfectly fine.

| Function | Description |
| -------- | ----------- |
| `SendMultipleMessagesWordList(timestamp, words)` | When supplied an `IIterable` of 32 bit unsigned integers, this sends more than one message with the same timestamp. Message words must be ordered contiguously from word-0 to word-n for each message, and the message types must be valid for the number of words for each message. All messages are sent with the same timestamp.|
| `SendMultipleMessagesWordArray(timestamp, startIndex, wordCount, words)` | Similar to the WordList approach, this will send multiple messages from an array, starting at the zero-based `startIndex` and continuing for `wordCount` words. The messages within that range must be valid and complete. All messages are sent with the same timestamp.|
| `SendMultipleMessagesPacketList(messages)` | Send an `IIterable` of `IMidiUniversalPacket` messages, each with their own timestamp. |
| `SendMultipleMessagesStructList(timestamp, messages)` | Send an `IIterable` of `MidiMessageStruct` messages. All messages are sent with the same timestamp|
| `SendMultipleMessagesStructArray(timestamp, startIndex, messageCount, messages)` | Send an an array of `MidiMessageStruct` messages, starting at `startIndex` and continuing for `messageCount` messages. All messages are sent with the same timestamp|
| `SendMultipleMessagesBuffer(timestamp, byteOffset, byteCount, buffer)` | Send multiple messages using the `IMemoryBuffer` approach and a single timestamp. |

When sending multiple messages, there is no implied all-or-nothing transaction. If an error is encountered when sending messages, the function stops processing the list at that point and returns a failure code, even if some messages were sent successfully.

> Tip: To learn more about how collections are handled in WinRT, and how they may convert to constructs like `std::vector`, see the [Collections with C++/WinRT](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/collections) page in our documentation.

## Other Functions

| Function | Description |
| -------- | ----------- |
| `Open()` | Open the connection and start receiving messages. Wire up the message event handler before calling this method. |
| `AddMessageProcessingPlugin(plugin)` | Add a message processing plugin to this connection |
| `RemoveMessageProcessingPlugin(id)` | Remove a message processing plugin from this connection |

## Events

| Event | Description |
| -------- | ----------- |
| `MessageReceived(source, args)` | From `IMidiMessageReceivedEventSource`. This is the event for receiving MIDI Messages, one at a time. |

When processing the `MessageReceived` event, do so quickly. This event is synchronous. If you need to do long-running processing of incoming messages, add them to your own incoming queue structure and have them processed by another application thread.

> Threading: Note that the thread the `MessageReceived` callback comes in on is not the same thread which created the connection to begin with. Windows MIDI Services uses a high-priority thread in the background, one per connection. For this reason, it's best to use the event only to receive the message and store it, not to do any additional processing on the message. The TAEF test `MidiEndpointCreationThreadTests` in the `Midi2.Client.unittests` project demonstrates how this works.

> Note: Wire up event handlers and add message processing plugins prior to calling `Open()`. 

## IDL

[MidiEndpointConnection IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiEndpointConnection.idl)

## Sample

Here's an excerpt from the full "API client basics" sample. It shows sending and receiving messages using the two built-in loopback endpoints. For more information on the loopback endpoints, see [diagnostics endpoints](../../endpoints/diagnostic-endpoints.md).

```cs
using (var session = MidiSession.CreateSession("API Sample Session"))
{
    // get the endpoint Ids. Normally, you'd use enumeration functions to get this
    // for non-diagnostics endpoints.
    var endpointAId = MidiDiagnostics.DiagnosticsLoopbackAEndpointId;
    var endpointBId = MidiDiagnostics.DiagnosticsLoopbackBEndpointId;

    Console.WriteLine("Connecting to Sender UMP Endpoint: " + endpointAId);
    Console.WriteLine("Connecting to Receiver UMP Endpoint: " + endpointBId);

    var sendEndpoint = session.CreateEndpointConnection(endpointAId);
    var receiveEndpoint = session.CreateEndpointConnection(endpointBId);

    void MessageReceivedHandler(object sender, MidiMessageReceivedEventArgs args)
    {
        var ump = args.GetMessagePacket();

        Console.WriteLine();
        Console.WriteLine("Received UMP");
        Console.WriteLine("- Current Timestamp: " + MidiClock.Now);
        Console.WriteLine("- UMP Timestamp:     " + ump.Timestamp);
        Console.WriteLine("- UMP Msg Type:      " + ump.MessageType);
        Console.WriteLine("- UMP Packet Type:   " + ump.PacketType);
        Console.WriteLine("- Message:           " + MidiMessageHelper.GetMessageFriendlyNameFromFirstWord(args.PeekFirstWord()));

        if (ump is MidiMessage32)
        {
            var ump32 = ump as MidiMessage32;

            if (ump32 != null)
                Console.WriteLine("- Word 0:            0x{0:X}", ump32.Word0);
        }
    };

    // wire up the event handler before opening the endpoint
    receiveEndpoint.MessageReceived += MessageReceivedHandler;

    Console.WriteLine("Opening endpoint connection");

    receiveEndpoint.Open();
    sendEndpoint.Open();

    Console.WriteLine("Creating MIDI 1.0 Channel Voice 32-bit UMP...");

    var ump32 = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
        MidiClock.Now, // use current timestamp
        5,      // group 4
        Midi1ChannelVoiceMessageStatus.NoteOn,  // 9
        3,      // channel 2
        120,    // note 120 - hex 0x78
        100);   // velocity 100 hex 0x64

    sendEndpoint.SendSingleMessagePacket((IMidiUniversalPacket)ump32);  // could also use the SendWords methods, etc.

    Console.WriteLine(" ** Wait for the message to arrive, and then press enter to cleanup. ** ");
    Console.ReadLine();

    // you should unregister the event handler as well
    receiveEndpoint.MessageReceived -= MessageReceivedHandler;

    // not strictly necessary if the session is going out of scope or is in a using block
    session.DisconnectEndpointConnection(sendEndpoint.ConnectionId);
    session.DisconnectEndpointConnection(receiveEndpoint.ConnectionId);
}

```
