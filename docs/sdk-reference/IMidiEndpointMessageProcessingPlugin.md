---
layout: sdk_reference_page
title: IMidiEndpointMessageProcessingPlugin
namespace: Windows.Devices.Midi2
type: interface
description: Interface implemented by any type which can be an endpoint processing plugin in the client WinRT API
---

Any type that implements this interface can be a message processing plugin. A plugin gets each incoming message from an endpoint connection and can act on it, change it, or stop it from going any further.

The API includes several plugins: `MidiVirtualDevice`, `MidiChannelEndpointListener`, `MidiGroupEndpointListener`, and `MidiMessageTypeEndpointListener`. They all implement this interface and work the same way. You can write your own, too.

Most of a plugin's work happens in `ProcessIncomingMessage`.

## Properties

| Property | Description |
| ---- | ---- |
| `PluginId` | A GUID for this plugin object. You need it to remove the plugin from the connection |
| `PluginName` | A name your application can give this plugin. Optional |
| `PluginTag` | Any extra data your application wants to keep with this plugin. Optional |
| `IsEnabled` | True if the plugin is turned on and should take part in handling messages |

## Functions

The connection waits for `ProcessIncomingMessage` to finish before it moves on, so keep it fast.

Applications are usually much faster than devices. But if your plugin can't keep up, the incoming message queue can fill up and cause errors. MIDI 2.0 has no speed limit for devices, and USB 3 and network devices, among others, can send a lot of messages in a very short time.

If you need to do slow work with incoming messages, copy them to your own queue and process them on another thread.

| Function | Description |
| ---- | ---- |
| `Initialize(endpointConnection)` | The connection calls this when the plugin is added. Do any setup that needs the connection here |
| `OnEndpointConnectionOpened()` | Called when the connection opens. If the plugin is added after the connection is already open, it's called right away |
| `ProcessIncomingMessage(args, skipFurtherListeners, skipMainMessageReceivedEvent)` | Called for each incoming message. Set `skipFurtherListeners` to true to keep the message from reaching any plugin after this one. Set `skipMainMessageReceivedEvent` to true to keep the connection from raising its own `MessageReceived` event for this message |
| `Cleanup()` | Called when the connection is shutting down |

## The two skip flags

The two flags control different things, and each one works on its own. `skipFurtherListeners` stops the message from reaching the rest of the plugins. `skipMainMessageReceivedEvent` stops the connection's own event. Setting one has no effect on the other, so a plugin that wants both must set both.

Both are output parameters, and each plugin reports only its own choice. Don't read the value that's passed in, and don't try to carry forward what an earlier plugin asked for. The connection combines the answers from every plugin itself, and a later plugin can't clear a flag an earlier plugin set. Set both flags on every path through your code, including the paths for messages you don't handle.
