---
layout: sdk_reference_page
title: IMidiMessageReceivedEventSource
namespace: Windows.Devices.Midi2
type: interface
description: Interface which contains the event definition used by any class which raises the `MessageReceived` event
---

Interface which contains the event definition used by any class which raises the `MessageReceived` event. This is defined in an interface so that message processing plugins and the `MidiEndpointConnection` type can be used interchangeably in an event handler.

## Events

The `MessageReceived` event is raised synchronously, and needs to be handled quickly and efficiently by the calling application.

Applications are typically much faster than devices at handling messages. However, failing to drain the incoming message queue fast enough can result in transmission errors. With MIDI 2.0 there is no upper performance limit on devices, and USB 3 and Network MIDI devices, among others, are capable of transmitting a large number of messages in a very short period of time.

If you need to do long-running processing of incoming messages, add them to your own incoming queue and have them processed by another application thread.

| Event | Description |
| -------- | ----------- |
| `MessageReceived(source, args)` | The main message received event definition. |

## Methods

| Method | Description |
| -------- | ----------- |
| `GetEndpointConnectionSource()` | Returns the [`IMidiEndpointConnectionSource`](./IMidiEndpointConnectionSource.md) that owns this message source. |

