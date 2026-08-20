---
layout: sdk_reference_page
title: MidiChannelEndpointListener
namespace: Windows.Devices.Midi2.ClientPlugins
type: runtimeclass
implements: Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin, Windows.Devices.Midi2.IMidiMessageReceivedEventSource
description: Provides a way to filter incoming messages by group and channel without opening separate connections
---

This class acts as a client-side filter. Incoming messages with the specified group and channel will be provided through the `MessageReceived` event. Other messages will be ignored. 

That means system real-time messages, SysEx messages, and any other messages without an explicit channel field (that we know about at API compile time) will be ignored here by default. If you want any of those, like the real-time messages like Clock, you can set up a `MidiMessageTypeEndpointListener` and have it listen to `MidiMessageType::SystemCommon32` or, just for real-time and system common messages, use the `IncludeSystemCommonAndRealTimeMessages` flag. Please refer to the MIDI UMP Specification for details on what types of messages are included in each message type. You could also set up a `MidiGroupEndpointListener` and get the entire stream of messages for that single group. That behaves most like a classic MIDI 1.0 API Port.

In addition to the properties and methods in `IMidiEndpointMessageProcessingPlugin`, and the `MessageReceived` event from `IMidiMessageReceivedEventSource` the class provides the following:

## Properties

| Property | Description |
| ---- | ---- |
| `IncludedGroup` | The single `MidiGroup` that this listener will listen to. If unspecified, all groups will be included in scope and only the channel will be evaluated. |
| `IncludedChannels` | The channels that this listener will listen to on the group. |
| `IncludeSystemCommonAndRealTimeMessages` | True if this plugin should fire MessageReceived events for system common/real-time messages like clock, which do not have a channel. False by default. |
| `PreventFiringMainMessageReceivedEvent` | True if this plugin should prevent the endpoint's `MessageReceived` event from firing if the message was in-scope for this plugin. |
| `PreventCallingFurtherListeners` | True if this plugin should prevent any plugins after this one from executing if the message was handled by this plugin instance. |

## Functions

| Property | Description |
| ---- | ---- |
| `MidiChannelEndpointListener()` | Construct a new instance of this type |

## Events

The `MessageReceived` event is raised synchronously, and needs to be handled quickly and efficiently by the calling application.

Applications are typically much faster than devices at handling messages. However, failing to drain the incoming message queue fast enough can result in transmission errors. With MIDI 2.0 there is no upper performance limit on devices, and USB 3 and Network MIDI devices, among others, are capable of transmitting a large number of messages in a very short period of time.

If you need to do long-running processing of incoming messages, add them to your own incoming queue and have them processed by another application thread.

| Event | Description |
| ---- | ---- |
| `MessageReceived (source, args)` | From `IMidiMessageReceivedEventSource`. Raised for each incoming message which is in scope for this listener. |

## Examples

```cpp
// set up your message receive handler and create your connection
// before setting up the individual message listeners. The event
// handler has the same signature as the main message received
// event on the connection.

midi2::MidiChannelEndpointListener channelsListener;

// listening to channels generally only makes sense if you also
// specify the group you are listening to.
channelsListener.IncludedGroup(midi2::MidiGroup(static_cast<uint8_t>(5)));

// add the channels you are listening to. Any messages which do 
// not have channels will not be raised through the event here.
channelsListener.IncludedChannels().Append(midi2::MidiChannel(static_cast<uint8_t>(3)));
channelsListener.IncludedChannels().Append(midi2::MidiChannel(static_cast<uint8_t>(7)));

// set this if you don't want the main message received event on the
// connection to fire for any messages this plugin handles.
channelsListener.PreventFiringMainMessageReceivedEvent(true);

auto channelMessagesReceivedEventToken = channelsListener.MessageReceived(MyMessageReceivedHandler);

myConnection.AddMessageProcessingPlugin(channelsListener);

// open after setting up the plugin so you don't miss any messages
myConnection.Open();

// ...
```

```csharp
// set up your message receive handler and create your connection
// before setting up the individual message listeners. The event
// handler has the same signature as the main message received
// event on the connection.

var channelsListener = new MidiChannelEndpointListener();

// listening to channels generally only makes sense if you also
// specify the group you are listening to.
channelsListener.IncludedGroup = new MidiGroup(5);

// add the channels you are listening to. Any messages which do 
// not have channels will not be raised through the event here.
channelsListener.IncludedChannels.Add(new MidiChannel(3));
channelsListener.IncludedChannels.Add(new MidiChannel(7));

// set this if you don't want the main message received event on the
// connection to fire for any messages this plugin handles.
channelsListener.PreventFiringMainMessageReceivedEvent = true;

channelsListener.MessageReceived += MyMessageReceivedHandler;

myConnection.AddMessageProcessingPlugin(channelsListener);

// open after setting up the plugin so you don't miss any messages
myConnection.Open();

// ...
```

More complete examples [available on Github](https://aka.ms/midirepo)
