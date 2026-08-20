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
| `ProcessIncomingMessage (args, skipFurtherListeners, skipMainMessageReceivedEvent)` | Callback for processing an incoming message. If the code sets `skipFurtherListeners` to true, any plugins after this one will not be called. If the code sets `skipMainMessageReceivedEvent` to true, the endpoint's MessageReceived event will not be called for this message. |
| `Cleanup()` | Called when the endpoint is tearing down |
