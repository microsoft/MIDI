---
layout: sdk_reference_page
title: IMidiEndpointConnectionSource
namespace: Windows.Devices.Midi2
type: interface
description: The parts of an endpoint connection that a message processing plugin can use
---

This interface gives a message processing plugin the parts of its connection that it needs. The WinRT API uses it so that the connection class and the plugin interface don't have to refer to each other directly, which would be a circular reference. Only `MidiEndpointConnection` implements it.

## Properties

| Property | Description |
| -------- | ----------- |
| `ConnectionId` | A GUID that identifies this connection |
| `ConnectedEndpointDeviceId` | The id of the endpoint this connection is for |
| `Settings` | The [`MidiEndpointConnectionSettings`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnectionSettings/) used to create this connection |
| `IsOpen` | True if the connection is open |
| `Tag` | Any extra object your application wants to keep with the connection |

## Events

| Event | Description |
| -------- | ----------- |
| `EndpointDeviceDisconnected(source, args)` | Raised when the endpoint device disconnects |
| `EndpointDeviceReconnected(source, args)` | Raised when the endpoint device reconnects, if automatic reconnection is turned on |

