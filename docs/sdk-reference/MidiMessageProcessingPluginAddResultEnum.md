---
layout: sdk_reference_page
title: MidiMessageProcessingPluginAddResult
namespace: Windows.Devices.Midi2
type: enum
description: Return value when adding a message processing plugin to a connection
---

`MidiEndpointConnection.AddMessageProcessingPlugin` returns this value. Check it. A plugin which was not added is not called, and because message processing plugins are how listeners and virtual devices receive messages, a plugin that was silently rejected looks exactly like an endpoint which is not sending anything.

## Properties

These values are not flags. Exactly one is returned.

| Property | Value | Description |
| -------- | ----- | ----------- |
| `Succeeded` | `0` | The plugin was added and initialized. |
| `FailedPluginIsNull` | `1` | The supplied plugin was null. |
| `FailedRawCallbackRegistered` | `2` | The connection has a COM Extensions messages received callback registered. That callback bypasses all message processing plugins, so the plugin would never be called. See below. |
| `FailedPluginAlreadyAdded` | `3` | A plugin with this same `PluginId` has already been added to this connection. Plugin ids are per instance, so this means the same plugin object was added twice, not that another plugin of the same type is present. |
| `FailedPluginInitializationError` | `4` | The plugin threw while being initialized. **The plugin is still in the connection's `MessageProcessingPlugins` collection**, but it may not be functional. Remove it with `RemoveMessageProcessingPlugin` if that matters to your application. |

## Message processing plugins and the COM Extensions are mutually exclusive

A connection uses either message processing plugins or a COM Extensions messages received callback. It cannot use both, because a registered callback bypasses all other incoming message handling, including every listener and the connection's own `MessageReceived` event.

The API enforces this from both directions rather than letting one silently disable the other:

- `AddMessageProcessingPlugin` returns `FailedRawCallbackRegistered` if a callback is already registered on the connection.
- [`SetMessagesReceivedCallback`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions) returns `E_ILLEGAL_STATE_CHANGE` if any plugins are already attached to the connection.

Whichever one is set up first wins. If you need to change approach on an existing connection, call `RemoveMessagesReceivedCallback` or `RemoveMessageProcessingPlugin` first.

This matters most for virtual devices. `MidiVirtualDevice` is itself a message processing plugin, so a virtual device application which also registers a COM callback would otherwise send correctly and never receive anything, with no error to explain it.

## Example

```cpp
midi2::ClientPlugins::MidiGroupEndpointListener listener;
listener.IncludedGroups().Append(midi2::MidiGroup(5));
listener.MessageReceived(MyMessageReceivedHandler);

if (connection.AddMessageProcessingPlugin(listener) != midi2::MidiMessageProcessingPluginAddResult::Succeeded)
{
    // the listener will never be called. Do not open and wait for messages which cannot arrive.
    return false;
}

// add plugins before opening, or messages which arrive early will not reach them
connection.Open();
```

More complete examples [available on Github](https://aka.ms/midirepo)
