---
layout: sdk_reference_page
title: MidiGroupEndpointListener
namespace: Windows.Devices.Midi2.ClientPlugins
type: runtimeclass
implements: Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin, Windows.Devices.Midi2.IMidiMessageReceivedEventSource
description: Provides a way to filter incoming messages by group without opening separate connections
---

This class acts as a filter. Incoming messages with the specified group will be provided through the `MessageReceived` event. Other messages will be ignored.

For a MIDI 1.0 device, where the ports (virtual MIDI cables) have been mapped to UMP groups, this class can provide the equivalent of a MIDI 1.0 port to an application, ignoring all other inputs and operating only on the included groups.

In addition to the properties and methods in `IMidiEndpointMessageProcessingPlugin`, and the MessageReceived event from `IMidiMessageReceivedEventSource` the class provides the properties and methods described below.

## Properties

| Property | Description |
| ---- | ---- |
| `IncludedGroups` | The list of `MidiGroup`s that this listener will listen to. |
| `PreventFiringMainMessageReceivedEvent` | True if this plugin should prevent the endpoint's `MessageReceived` event from firing if the message was in-scope for this plugin. |
| `PreventCallingFurtherListeners` | True if this plugin should prevent any plugins after this one from executing if the message was handled by this plugin instance. |

## Functions

| Property | Description |
| ---- | ---- |
| `MidiGroupEndpointListener()` | Construct a new instance of this type |

## Events

The `MessageReceived` event is raised synchronously, and needs to be handled quickly and efficiently by the calling application.

Applications are typically much faster than devices at handling messages. However, failing to drain the incoming message queue fast enough can result in transmission errors. With MIDI 2.0 there is no upper performance limit on devices, and USB 3 and Network MIDI devices, among others, are capable of transmitting a large number of messages in a very short period of time.

If you need to do long-running processing of incoming messages, add them to your own incoming queue and have them processed by another application thread.

| Event | Description |
| ---- | ---- |
| `MessageReceived (source, args)` | From `IMidiMessageReceivedEventSource`. Raised for each incoming message which is in scope for this listener. |

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

More complete examples [available on Github](https://aka.ms/midirepo)

## Samples

This is how you emulate a WinMM port. One connection carries up to 16 groups in each direction, so
an application which presents ports to its users has to filter. Note `PreventFiringMainMessageReceivedEvent`
in these samples: set it when you also handle the connection's own `MessageReceived`, or every
message arrives twice.

* [C++/WinRT endpoint-listeners](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/endpoint-listeners)
* [C# endpoint-listeners](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/endpoint-listeners)
