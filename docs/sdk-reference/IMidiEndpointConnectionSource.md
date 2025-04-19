---
layout: sdk_reference_page
title: IMidiEndpointConnectionSource
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: interface
description: Interface which is used to prevent circular references in the SDK, specifically with message processing plugins
---

Interface which is used to prevent circular references in the SDK, specifically with message processing plugins. This interface is only supported when used by the `MidiEndpointConnection` class.

## Events

| Event | Description |
| -------- | ----------- |
| `EndpointDeviceDisconnected(source, args)` | Raised when the endpoint device has disconnected. |
| `EndpointDeviceReconnected(source, args)` | Raised when the endpoint device has reconnected, when automatic reconnection is enabled. |

