---
layout: api_page
title: IMidiMessageReceivedEventSource
parent: Connections
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# IMidiMessageReceivedEventSource

Interface which contains the event definition used by any class which raises the `MessageReceived` event. This is defined in an interface so that message processing plugins and the `MidiEndpointConnection` type can be used interchangeably in an event handler.

## Events

| Event | Description |
| -------- | ----------- |
| `MessageReceived(source, args)` | The main message received event definition. |

## IDL

[IMidiMessageReceivedEventSource IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/IMidiMessageReceivedEventSource.idl)

