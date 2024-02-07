---
layout: api_page
title: MidiChannelEndpointListener
parent: Client Plugins
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiChannelEndpointListener

This class acts as a filter. Incoming messages with the specified group and channel will be provided through the `MessageReceived` event. Other messages will be ignored.

In addition to the properties and methods in `IMidiEndpointMessageProcessingPlugin`, and the MessageReceived event from `IMidiMessageReceivedEventSource` the class provides the following:

## Properties

| Property | Description |
| ---- | ---- |
| `IncludeGroup` | The `MidiGroup` that this listener will listen to. |
| `IncludeChannels` | The channels that this listener will listen to on the group. |
| `PreventCallingFurtherListeners` | True if this plugin should prevent further listeners from processing a message that is in-scope for this processor. |
| `PreventFiringMainMessageReceivedEvent` | True if this plugin should prevent the endpoint's `MessageReceived` event from firing if the message was in-scope for this plugin. |

## Functions

| Property | Description |
| ---- | ---- |
| `MidiChannelEndpointListener()` | Construct a new instance of this type |

## IDL

[MidiChannelEndpointListener IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiChannelEndpointListener.idl)
