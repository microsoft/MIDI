---
layout: sdk_reference_page
title: IMidiEndpointMessageProcessingPlugin
namespace: Windows.Devices.Midi2
type: interface
description: Interface implemented by any type which can be an endpoint processing plugin in the client app SDK
---

This interface is implemented by any type which can be an endpoint processing plugin in the client app SDK. These plugins are used to process or manipulate messages coming from an endpoint. 

Microsoft provides several plugins in the API, including the `MidiVirtualEndpointDevice`, the `MidiChannelEndpointListener`, and the `MidiGroupEndpointListener`. All of these types implement the `IMidiEndpointMessageProcessingPlugin` interface and operate in the same way.

The main part of message processing is the `ProcessIncomingMessage` callback.

## Properties

| Property | Description |
| ---- | ---- |
| `PluginId` | Generated GUID for this plugin instance. This is needed if you want to remove the plugin from the endpoint connection |
| `PluginName` | Optional application-supplied name for this plugin instance. |
| `PluginTag` | Optional application-supplied arbitrary data to associate with this plugin instance |
| `IsEnabled` | True if the plugin is enabled and should participate in message processing |

## Functions

The `ProcessIncomingMessage` callback is synchronous, and needs to be handled quickly and efficiently by the calling application.

Applications are typically much faster than devices at handling messages. However, failing to drain the incoming message queue fast enough can result in transmission errors. With MIDI 2.0 there is no upper performance limit on devices, and USB 3 and Network MIDI devices, among others, are capable of transmitting a large number of messages in a very short period of time.

If you need to do long-running processing of incoming messages, add them to your own incoming queue and have them processed by another application thread.

| Function | Description |
| ---- | ---- |
| `Initialize (endpointConnection)` | Called by the endpoint connection. Perform any setup code which requires the endpoint connection pointer here. |
| `OnEndpointConnectionOpened()` | Callback when the endpoint connection is opened. If the plugin is added after the endpoint connection has already been opened, this is called immediately. |
| `ProcessIncomingMessage (args, skipFurtherListeners, skipMainMessageReceivedEvent)` | Callback for processing an incoming message. Set `skipFurtherListeners` to true to stop the message being passed to any plugin after this one. Set `skipMainMessageReceivedEvent` to true to stop the endpoint connection raising its own `MessageReceived` event for this message. |
| `Cleanup()` | Called when the endpoint is tearing down |

## The two skip flags

They suppress different things and are independent of each other. `skipFurtherListeners` ends the plugin chain for this message; `skipMainMessageReceivedEvent` suppresses the connection's own event. Setting either one has no effect on the other, so a plugin which wants both must set both.

Both are output parameters, and each plugin reports only its own decision. Do not read the value passed in and do not try to carry forward what an earlier plugin asked for — the endpoint connection combines the answers from every plugin itself, and a value set by one plugin cannot be cleared by a later one. Write both on every path through your implementation, including the paths where the message is not one you handle.
