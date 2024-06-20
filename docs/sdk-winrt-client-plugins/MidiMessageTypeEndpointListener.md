---
layout: api_page
title: MidiMessageTypeEndpointListener
parent: Midi2.ClientPlugins
has_children: false
---

# MidiMessageTypeEndpointListener

This class acts as a filter. Incoming messages with the specified message type will be provided through the `MessageReceived` event. Other messages will be ignored. In this way, the listener can be set up to, for example, only pay attention to MIDI 2.0 Channel Voice messages, leaving stream messages and System Exclusive by the wayside.

In addition to the properties and methods in `IMidiEndpointMessageProcessingPlugin`, and the MessageReceived event from `IMidiMessageReceivedEventSource` the class provides the following:

## Properties

| Property | Description |
| ---- | ---- |
| `IncludedMessageTypes` | The list of `MidiMessageType` values that this listener will listen to. |
| `PreventCallingFurtherListeners` | True if this plugin should prevent further listeners from processing a message that is in-scope for this processor. |
| `PreventFiringMainMessageReceivedEvent` | True if this plugin should prevent the endpoint's `MessageReceived` event from firing if the message was in-scope for this plugin. |

## Functions

| Property | Description |
| ---- | ---- |
| `MidiMessageTypeEndpointListener()` | Construct a new instance of this type |

## IDL

[MidiMessageTypeEndpointListener IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-client-plugins/MidiMessageTypeEndpointListener.idl)
