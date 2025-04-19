---
layout: sdk_reference_page
title: IMidiMessageReceivedEventSource
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: interface
description: Interface which contains the event definition used by any class which raises the `MessageReceived` event
---

Interface which contains the event definition used by any class which raises the `MessageReceived` event. This is defined in an interface so that message processing plugins and the `MidiEndpointConnection` type can be used interchangeably in an event handler.

## Events

| Event | Description |
| -------- | ----------- |
| `MessageReceived(source, args)` | The main message received event definition. |

