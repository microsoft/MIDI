# MidiEndpointConnection

The `MidiEndpointConnection` type represents a single connection to a single endpoint managed by Windows MIDI Services. It is created using the functions of the `MidiSession`, and is tied to the lifetime of that session.

Connections allocate resources including send/receive buffers, and processing threads. For that reason, a session should generally not open more than one connection to a single endpoint. If you need to partition out messages more easily (by group or channel, for example) the `MessageProcessingPlugins` collection will help you do that.

## Properties

| Property | Description |
| -------- | ----------- |
| ConnectionId | The generated GUID which uniquely identifes this connection instance. This is what is provided to the `MidiSession` when disconnecting an endpoint |
| EndpointDeviceId | The system-wide identifier for the device connection. This is returned through enumeration calls. |
| Tag | You may use this `Tag` property to hold any additional information you wish to have associated with the connection. |
| IsOpen | True if this connection is currently open. When first created, the connection is not open until the consuming code calls the `Open` method |
| Settings | Settings used to create this connection. |
| MessageProcessingPlugins | Collection of all message processing plugins which will optionally handle incoming messages. Fill this collection before opening the connection. Once the connection has been opened, plugins added to this collection are not initialized properly and have undefined behavior. |

## Static Member Functions

| Static Function | Description |
| -------- | ----------- |
| GetDeviceSelector() | Returns the device selector used for enumerating endpoint devices compatible with this API. |
| SendMessageSucceeded(sendResult) | Helper function to decipher the return result of a message sending function to tell if it succeeded. |
| SendMessageFailed(sendResult) | Helper function to decipher the return result of a message sending function to tell if it failed. |

## Functions

| Function | Description |
| -------- | ----------- |
| Open() | Open the connection and start receiving messages. Wire up the message event handler before calling this method. |
| SendMessagePacket(message) | Send an `IMidiUniversalPacket`-implementing type such as `MidiMessage64` or a strongly-typed message class. |
| SendMessageStruct(timestamp, message, wordCount) | Send a fixed-sized `MidiMessageStruct` containing `wordCount` valid words. Additional words are ignored. |
| SendMessageWordArray(timestamp, words, startIndex, wordCount) | Note: Some projections will send the entire array as a copy, so this may not be the most effecient way to send messages from your language. |
| SendMessageWords(timestamp, word0) | Send a single 32-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| SendMessageWords(timestamp, word0, word1) | Send a single 64-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| SendMessageWords(timestamp, word0, word1, word2) | Send a single 96-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| SendMessageWords(timestamp, word0, word1, word2, word3) | Send a single 128-bit Universal MIDI Packet as 32-bit words. This is often the most efficient way to send this type of message |
| SendMessageBuffer(timestamp, buffer, byteOffset, byteLength) | Send a single Universal MIDI Packet as bytes from a buffer. The number of bytes sent must match the size read from the first 4 bits of the data starting at the specified offset, and must be laid out correctly with the first byte corresponding to the MSB of the first word of the UMP (the word which contains hte message type). If you want to manage a chunk of buffer memory, the `IMemoryBuffer` type is the acceptable WinRT approach, and is as close as you get to sending a pointer into a buffer. |
| Close() | Please see note below about calling `Close` or the projected `Dispose` function |

> Tip: In all the functions which accept a timestamp to schedule the message, **you can send a timestamp of 0 (zero) to bypass the scheduler and send the message immediately**. Otherwise, the provided timestamp is treated as an absolute time for when the message should be sent from the service. Note that the service-based scheduler (currently based on a `std::priority_queue`) gets less efficient when there are thousands of messages in it, so it's recommended that you not schedule too many messages at a time or too far out into the future. 

## Events

| Event | Description |
| -------- | ----------- |
| MessageReceived(source, args) | From `IMidiMessageReceivedEventSource`. This is the event for receiving MIDI Messages, one at a time. |

When processing the `MessageReceived` event, do so quickly. This event is synchronous. If you need to do long-running processing of incoming messages, add them to your own incoming queue structure and have them processed by another application thread.

> Note: If you manually close a `MidiEndpointConnection` using `IClosable` (or `IDisposable`), it will not be removed from the `MidiSession`'s collection of endpoints. Instead, use the `DisconnectEndpointConnection` method of the `MidiSession` to keep both in sync. For that reason, we do not recommend that you wrap the `CreateEndpointConnection` calls in a using statement.

> Note: Wire up event handlers and add all message processing plugins prior to calling `Open()`. 

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiEndpointConnection.idl)
