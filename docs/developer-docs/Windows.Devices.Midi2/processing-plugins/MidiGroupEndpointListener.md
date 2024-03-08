---
layout: api_page
title: MidiGroupEndpointListener
parent: Client Plugins
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiGroupEndpointListener

This class acts as a filter. Incoming messages with the specified group will be provided through the `MessageReceived` event. Other messages will be ignored.

For a MIDI 1.0 device, where the ports (virtual MIDI cables) have been mapped to UMP groups, this class can provide the equivalent of a MIDI 1.0 port to an application, ignoring all other inputs and operating only on the included groups.

In addition to the properties and methods in `IMidiEndpointMessageProcessingPlugin`, and the MessageReceived event from `IMidiMessageReceivedEventSource` the class provides the following:

## Properties

| Property | Description |
| ---- | ---- |
| `IncludeGroups` | The list of `MidiGroup` numbers that this listener will listen to. |
| `PreventCallingFurtherListeners` | True if this plugin should prevent further listeners from processing a message that is in-scope for this processor. |
| `PreventFiringMainMessageReceivedEvent` | True if this plugin should prevent the endpoint's `MessageReceived` event from firing if the message was in-scope for this plugin. |

## Functions

| Property | Description |
| ---- | ---- |
| `MidiGroupEndpointListener()` | Construct a new instance of this type |


```cpp
// set up your message receive handler and create your connection
// before setting up the individual message listeners. The event
// handler has the same signature as the main message received
// event on the connection.

midi2::MidiGroupEndpointListener groupsListener;
groupsListener.IncludeGroups().Append(midi2::MidiGroup(5));
groupsListener.IncludeGroups().Append(midi2::MidiGroup(6));

// set this if you don't want the main message received event on the
// connection to fire for any messages this plugin handles.
groupsListener.PreventFiringMainMessageReceivedEvent(true);

auto groupsMessagesReceivedEventToken = groupsListener.MessageReceived(MyMessageReceivedHandler);

myConnection.AddMessageProcessingPlugin(groupsListener);

// open after setting up the plugin so you don't miss any messages
myConnection.Open();

// ...
```

## IDL

[MidiGroupEndpointListener IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiGroupEndpointListener.idl)
