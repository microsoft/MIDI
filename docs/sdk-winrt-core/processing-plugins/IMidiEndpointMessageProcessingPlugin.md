---
layout: api_page
title: IMidiEndpointMessageProcessingPlugin
parent: Client Plugins
grand_parent: Midi2 core
has_children: false
---

# IMidiEndpointMessageProcessingPlugin

This interface is implemented by any type which can be an endpoint processing plugin in the client API. These plugins are used to process or manipulate messages coming from an endpoint. 

Microsoft provides several plugins in the API, including the `MidiVirtualEndpointDevice`, the `MidiChannelEndpointListener`, and the `MidiGroupEndpointListener`. All of these types implement the `IMidiEndpointMessageProcessingPlugin` interface and operate in the same way.

The main part of message processing is the `ProcessIncomingMessage` callback.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Property | Description |
| ---- | ---- |
| `Id` | Generated GUID for this plugin instance. This is needed if you want to remove the plugin from the endpoint connection |
| `Name` | Optional application-supplied name for this plugin instance. |
| `Tag` | Optional application-supplied arbitrary data to associate with this plugin instance |
| `IsEnabled` | True if the plugin is enabled and should participate in message processing |

## Functions

| Function | Description |
| ---- | ---- |
| `Initialize(endpointConnection)` | Called by the endpoint connection. Perform any setup code which requires the endpoint connection pointer here. |
| `OnEndpointConnectionOpened()` | Callback when the endpoint connection is opened. If the plugin is added after the endpoint connection has already been opened, this is called immediately. |
| `ProcessIncomingMessage(args, skipFurtherListeners, skipMainMessageReceivedEvent)` | Callback for processing an incoming message. If the code sets `skipFurtherListeners` to true, any plugins after this one will not be called. If the code sets `skipMainMessageReceivedEvent` to true, the endpoint's MessageReceived event will not be called for this message. Note: this callback is synchronous, so code in this should execute quickly and return immediately when complete. |
| `Cleanup()` | Called when the endpoint is tearing down |

## IDL

[IMidiEndpointMessageProcessingPlugin IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/IMidiEndpointMessageProcessingPlugin.idl)
