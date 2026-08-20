---
layout: sdk_reference_page
title: MidiMessageTypeEndpointListener
namespace: Windows.Devices.Midi2.ClientPlugins
type: runtimeclass
implements: Windows.Devices.Midi2.IMidiEndpointMessageProcessingPlugin, Windows.Devices.Midi2.IMidiMessageReceivedEventSource
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
| `PreventCallingFurtherListeners` | True if this plugin should prevent any plugins after this one from executing if the message was handled by this plugin instance. |

## Functions

| Property | Description |
| ---- | ---- |
| `MidiMessageTypeEndpointListener()` | Construct a new instance of this type |

## Events

The `MessageReceived` event is raised synchronously, and needs to be handled quickly and efficiently by the calling application.

Applications are typically much faster than devices at handling messages. However, failing to drain the incoming message queue fast enough can result in transmission errors. With MIDI 2.0 there is no upper performance limit on devices, and USB 3 and Network MIDI devices, among others, are capable of transmitting a large number of messages in a very short period of time.

If you need to do long-running processing of incoming messages, add them to your own incoming queue and have them processed by another application thread.

| Event | Description |
| ---- | ---- |
| `MessageReceived (source, args)` | From `IMidiMessageReceivedEventSource`. Raised for each incoming message which is in scope for this listener. |

## Example

More complete examples [available on Github](https://aka.ms/midirepo)
