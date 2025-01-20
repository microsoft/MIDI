---
layout: api_page
title: IMidiEndpointConnectionSource
parent: Connections
grand_parent: Midi2 core
has_children: false
---

# IMidiEndpointConnectionSource

Interface which is used to prevent circular references in the SDK, specifically with message processing plugins. This interface is only supported when used by the `MidiEndpointConnection` class.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Events

| Event | Description |
| -------- | ----------- |
| `EndpointDeviceDisconnected(source, args)` | Raised when the endpoint device has disconnected. |
| `EndpointDeviceReconnected(source, args)` | Raised when the endpoint device has reconnected, when automatic reconnection is enabled. |

## IDL

[IMidiEndpointConnectionSource IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/IMidiEndpointConnectionSource.idl)

