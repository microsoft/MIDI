---
layout: sdk_reference_page
title: MidiMessageProcessingPluginAddResult
namespace: Windows.Devices.Midi2
type: enum
description: Return value when adding a message processing plugin to a connection
---

`MidiEndpointConnection.AddMessageProcessingPlugin` returns this value. Always check it. A plugin that wasn't added is never called. Listeners and virtual devices get their messages through message processing plugins, so a plugin that was turned away looks exactly like an endpoint that isn't sending anything.

## Properties

These values aren't flags. Exactly one is returned.

| Property | Value | Description |
| -------- | ----- | ----------- |
| `Succeeded` | `0` | The plugin was added and initialized. |
| `FailedPluginIsNull` | `1` | The plugin you passed was null. |
| `FailedRawCallbackRegistered` | `2` | The connection has a COM extensions messages-received callback registered. That callback skips every message processing plugin, so this plugin would never be called. See below. |
| `FailedPluginAlreadyAdded` | `3` | A plugin with the same `PluginId` was already added to this connection. Each plugin object has its own id, so this means the same object was added twice, not that another plugin of the same type is there. |
| `FailedPluginInitializationError` | `4` | The plugin threw an exception while it was being initialized. **The plugin is still in the connection's `MessageProcessingPlugins` collection**, but it may not work. If that matters to your application, remove it with `RemoveMessageProcessingPlugin`. |

## Message processing plugins or a COM callback, not both

A connection can use message processing plugins or a COM extensions messages-received callback, but not both. A registered callback skips all other handling of incoming messages, including every listener and the connection's own `MessageReceived` event.

The API checks this both ways, so one can't quietly switch off the other:

- `AddMessageProcessingPlugin` returns `FailedRawCallbackRegistered` if a callback is already registered on the connection.
- [`SetMessagesReceivedCallback`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions) returns `E_ILLEGAL_STATE_CHANGE` if any plugins are already attached to the connection.

Whichever you set up first wins. To switch on a connection you already have, first call `RemoveMessagesReceivedCallback` or `RemoveMessageProcessingPlugin`.

This matters most for virtual devices. `MidiVirtualDevice` is itself a message processing plugin. Without this check, a virtual device application that also registered a COM callback would send fine but never receive anything, and no error would say why.

## Example

```cpp
midi2::ClientPlugins::MidiGroupEndpointListener listener;
listener.IncludedGroups().Append(midi2::MidiGroup(5));
listener.MessageReceived(MyMessageReceivedHandler);

if (connection.AddMessageProcessingPlugin(listener) != midi2::MidiMessageProcessingPluginAddResult::Succeeded)
{
    // the listener will never be called, so don't open and wait for messages that can't arrive
    return false;
}

// add plugins before opening, or messages that arrive early won't reach them
connection.Open();
```

More complete examples are [available on GitHub](https://aka.ms/midirepo)
