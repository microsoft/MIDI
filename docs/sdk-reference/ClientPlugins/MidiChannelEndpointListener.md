---
layout: sdk_reference_page
title: MidiChannelEndpointListener
namespace: Windows.Devices.Midi2.ClientPlugins
type: runtimeclass
implements: Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin, Windows.Devices.Midi2.IMidiMessageReceivedEventSource
description: Provides a way to filter incoming messages by group and channel without opening separate connections
---

This class filters incoming messages in your application. Messages on the group and channels you choose are passed to its `MessageReceived` event, and other messages are ignored.

So by default, it ignores system real-time messages, System Exclusive messages, and any other message that has no channel field, as far as this API knows. If you want some of those, you have three choices:

- For system common and real-time messages, such as Timing Clock, set `IncludeSystemCommonAndRealTimeMessages`.
- Add a `MidiMessageTypeEndpointListener` that listens for `MidiMessageType::SystemCommon32`, or any other message type. The UMP specification says which messages each message type includes.
- Add a `MidiGroupEndpointListener` to get every message for one group. That works most like a port in the older MIDI 1.0 APIs.

Along with everything in `IMidiEndpointMessageProcessingPlugin`, and the `MessageReceived` event from `IMidiMessageReceivedEventSource`, this class has:

## Properties

| Property | Description |
| ---- | ---- |
| `IncludedGroup` | The one `MidiGroup` this listener listens to. If you don't set it, every group is included, and only the channel is checked |
| `IncludedChannels` | The channels this listener listens to on the group |
| `IncludeSystemCommonAndRealTimeMessages` | True if this listener should also raise `MessageReceived` for system common and real-time messages, such as Timing Clock, which have no channel. False by default |
| `PreventFiringMainMessageReceivedEvent` | True to stop the connection's own `MessageReceived` event from firing for the messages this listener handles |
| `PreventCallingFurtherListeners` | True to keep plugins after this one from getting the messages this listener handles |

## Constructors

| Constructor | Description |
| ---- | ---- |
| `MidiChannelEndpointListener()` | Creates a new listener |

## Events

The listener waits for your `MessageReceived` handler to finish before it moves on, so keep your handler fast.

Applications are usually much faster than devices. But if your handler can't keep up, the incoming message queue can fill up and cause errors. MIDI 2.0 has no speed limit for devices, and USB 3 and network devices, among others, can send a lot of messages in a very short time.

If you need to do slow work with incoming messages, copy them to your own queue and process them on another thread.

| Event | Description |
| ---- | ---- |
| `MessageReceived(source, args)` | From `IMidiMessageReceivedEventSource`. Raised for each incoming message this listener is set to pass on |

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

// a plugin which was not added is never called, so check the result
if (myConnection.AddMessageProcessingPlugin(channelsListener) != midi2::MidiMessageProcessingPluginAddResult::Succeeded)
{
    return;
}

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

// a plugin which was not added is never called, so check the result
if (myConnection.AddMessageProcessingPlugin(channelsListener) != MidiMessageProcessingPluginAddResult.Succeeded)
{
    return;
}

// open after setting up the plugin so you don't miss any messages
myConnection.Open();

// ...
```

More complete examples are [available on GitHub](https://aka.ms/midirepo)
