---
layout: sdk_reference_page
title: IMidiMessageReceivedEventSource
namespace: Windows.Devices.Midi2
type: interface
description: Interface for any class that raises the MessageReceived event
---

Any class that raises the `MessageReceived` event implements this interface. Because message processing plugins and `MidiEndpointConnection` share it, one event handler can work with either.

## Events

The source waits for your `MessageReceived` handler to finish before it moves on, so keep your handler fast.

Applications are usually much faster than devices. But if your handler can't keep up, the incoming message queue can fill up and cause errors. MIDI 2.0 has no speed limit for devices, and USB 3 and network devices, among others, can send a lot of messages in a very short time.

If you need to do slow work with incoming messages, copy them to your own queue and process them on another thread.

| Event | Description |
| -------- | ----------- |
| `MessageReceived(source, args)` | Raised for each incoming message. `args` is a `MidiMessageReceivedEventArgs` |

## Methods

| Method | Description |
| -------- | ----------- |
| `GetEndpointConnectionSource()` | Returns the [`IMidiEndpointConnectionSource`]({{ site.baseurl }}/sdk-reference/IMidiEndpointConnectionSource/) that owns this message source. |

