---
layout: sdk_reference_page
title: IMidiEndpointConnectionSource
namespace: Windows.Devices.Midi2
type: interface
description: Interface which is used to prevent circular references in the SDK, specifically with message processing plugins
---

Interface which is used to prevent circular references in the SDK, specifically with message processing plugins. This interface is only supported when used by the `MidiEndpointConnection` class.

## Properties

| Property | Description |
| -------- | ----------- |
| `ConnectionId` | The generated GUID which uniquely identifies this connection instance. |
| `ConnectedEndpointDeviceId` | The system-wide identifier for the endpoint device. |
| `Settings` | The [`MidiEndpointConnectionSettings`](./MidiEndpointConnectionSettings.md) used to create this connection. |
| `IsOpen` | True if this connection is currently open. |
| `Tag` | Optional application-supplied object associated with this connection. |

## Events

| Event | Description |
| -------- | ----------- |
| `EndpointDeviceDisconnected(source, args)` | Raised when the endpoint device has disconnected. |
| `EndpointDeviceReconnected(source, args)` | Raised when the endpoint device has reconnected, when automatic reconnection is enabled. |

