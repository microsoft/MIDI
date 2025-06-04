---
layout: sdk_reference_page
title: MidiChannelEndpointListener
namespace: Microsoft.Windows.Devices.Midi2.ClientPlugins
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Microsoft.Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin, Microsoft.Windows.Devices.Midi2.IMidiMessageReceivedEventSource
description: Provides a way to filter incoming messages by group and channel without opening separate connections
---

This class acts as a client-side filter. Incoming messages with the specified group and channel will be provided through the `MessageReceived` event. Other messages will be ignored.

In addition to the properties and methods in `IMidiEndpointMessageProcessingPlugin`, and the MessageReceived event from `IMidiMessageReceivedEventSource` the class provides the following:

## Properties

| Property | Description |
| ---- | ---- |
| `IncludedGroup` | The `MidiGroup` that this listener will listen to. |
| `IncludedChannels` | The channels that this listener will listen to on the group. |
| `PreventCallingFurtherListeners` | True if this plugin should prevent further listeners from processing a message that is in-scope for this processor. |
| `PreventFiringMainMessageReceivedEvent` | True if this plugin should prevent the endpoint's `MessageReceived` event from firing if the message was in-scope for this plugin. |

## Functions

| Property | Description |
| ---- | ---- |
| `MidiChannelEndpointListener()` | Construct a new instance of this type |

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
