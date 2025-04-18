---
layout: sdk_reference_page
title: MidiMessageTypeEndpointListener
namespace: Microsoft.Windows.Devices.Midi2.ClientPlugins
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: Microsoft.Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin, Microsoft.Windows.Devices.Midi2.IMidiMessageReceivedEventSource
description: Provides a way to filter incoming messages by message type
---

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

## Example

More complete examples [available on Github](https://aka.ms/midirepo)
