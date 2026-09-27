---
layout: sdk_reference_page
title: MidiMessageTypeEndpointListener
namespace: Windows.Devices.Midi2.ClientPlugins
type: runtimeclass
implements: Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin, Windows.Devices.Midi2.IMidiMessageReceivedEventSource
description: Provides a way to filter incoming messages by message type
---

This class filters incoming messages in your application. Messages of the types you choose are passed to its `MessageReceived` event, and other messages are ignored. For example, you can set it to pass on only MIDI 2.0 channel voice messages, and skip stream messages and System Exclusive.

Along with everything in `IMidiEndpointMessageProcessingPlugin`, and the `MessageReceived` event from `IMidiMessageReceivedEventSource`, this class has:

## Properties

| Property | Description |
| ---- | ---- |
| `IncludedMessageTypes` | The message types (`MidiMessageType`) this listener listens to |
| `PreventFiringMainMessageReceivedEvent` | True to stop the connection's own `MessageReceived` event from firing for the messages this listener handles |
| `PreventCallingFurtherListeners` | True to keep plugins after this one from getting the messages this listener handles |

## Constructors

| Constructor | Description |
| ---- | ---- |
| `MidiMessageTypeEndpointListener()` | Creates a new listener |

## Events

The listener waits for your `MessageReceived` handler to finish before it moves on, so keep your handler fast.

Applications are usually much faster than devices. But if your handler can't keep up, the incoming message queue can fill up and cause errors. MIDI 2.0 has no speed limit for devices, and USB 3 and network devices, among others, can send a lot of messages in a very short time.

If you need to do slow work with incoming messages, copy them to your own queue and process them on another thread.

| Event | Description |
| ---- | ---- |
| `MessageReceived(source, args)` | From `IMidiMessageReceivedEventSource`. Raised for each incoming message this listener is set to pass on |

## Examples

* [C++/WinRT endpoint-listeners](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/endpoint-listeners)
* [C# endpoint-listeners](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/endpoint-listeners)

More complete examples are [available on GitHub](https://aka.ms/midirepo)
