---
layout: sdk_reference_page
title: MidiGroupEndpointListener
namespace: Windows.Devices.Midi2.ClientPlugins
type: runtimeclass
implements: Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin, Windows.Devices.Midi2.IMidiMessageReceivedEventSource
description: Provides a way to filter incoming messages by group without opening separate connections
---

This class filters incoming messages in your application. Messages on the groups you choose are passed to its `MessageReceived` event, and other messages are ignored.

On a MIDI 1.0 device, each port (virtual MIDI cable) becomes a UMP group. So this class can give your application the same thing as a MIDI 1.0 port: only the messages for the groups you include.

Along with everything in `IMidiEndpointMessageProcessingPlugin`, and the `MessageReceived` event from `IMidiMessageReceivedEventSource`, this class has:

## Properties

| Property | Description |
| ---- | ---- |
| `IncludedGroups` | The groups (`MidiGroup`) this listener listens to |
| `PreventFiringMainMessageReceivedEvent` | True to stop the connection's own `MessageReceived` event from firing for the messages this listener handles |
| `PreventCallingFurtherListeners` | True to keep plugins after this one from getting the messages this listener handles |

## Constructors

| Constructor | Description |
| ---- | ---- |
| `MidiGroupEndpointListener()` | Creates a new listener |

## Events

The listener waits for your `MessageReceived` handler to finish before it moves on, so keep your handler fast.

Applications are usually much faster than devices. But if your handler can't keep up, the incoming message queue can fill up and cause errors. MIDI 2.0 has no speed limit for devices, and USB 3 and network devices, among others, can send a lot of messages in a very short time.

If you need to do slow work with incoming messages, copy them to your own queue and process them on another thread.

| Event | Description |
| ---- | ---- |
| `MessageReceived(source, args)` | From `IMidiMessageReceivedEventSource`. Raised for each incoming message this listener is set to pass on |

## Example

```cpp
// set up your message receive handler and create your connection
// before setting up the individual message listeners. The event
// handler has the same signature as the main message received
// event on the connection.

midi2::MidiGroupEndpointListener groupsListener;
groupsListener.IncludedGroups().Append(midi2::MidiGroup(static_cast<uint8_t>(5)));
groupsListener.IncludedGroups().Append(midi2::MidiGroup(static_cast<uint8_t>(6)));

// set this if you don't want the main message received event on the
// connection to fire for any messages this plugin handles.
groupsListener.PreventFiringMainMessageReceivedEvent(true);

auto groupsMessagesReceivedEventToken = groupsListener.MessageReceived(MyMessageReceivedHandler);

// a plugin which was not added is never called, so check the result
if (myConnection.AddMessageProcessingPlugin(groupsListener) != midi2::MidiMessageProcessingPluginAddResult::Succeeded)
{
    return;
}

// open after setting up the plugin so you don't miss any messages
myConnection.Open();

// ...
```

More complete examples are [available on GitHub](https://aka.ms/midirepo)

## Samples

This is how you imitate a WinMM port. One connection carries up to 16 groups in each direction, so an application that shows ports to its users has to filter them. Notice `PreventFiringMainMessageReceivedEvent` in these samples. Set it when you also handle the connection's own `MessageReceived`, or every message arrives twice.

* [C++/WinRT endpoint-listeners](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/endpoint-listeners)
* [C# endpoint-listeners](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/endpoint-listeners)
