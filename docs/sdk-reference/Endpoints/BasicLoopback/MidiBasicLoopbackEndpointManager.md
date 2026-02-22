---
layout: sdk_reference_page
title: MidiBasicLoopbackEndpointManager
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: The primary class used to create or remove basic MIDI 1.0 loopback endpoints
---

## Static Properties

| Function | Description |
| -------- | ----------- |
| `IsTransportAvailable` | Returns true if this transport is available in the service. |
| `TransportId` | Returns the GUID of this transport. |

## Static Functions

| Function | Description |
| -------- | ----------- |
| `CreateTransientLoopbackEndpoint (MidiBasicLoopbackEndpointCreationConfig)` | Create a loopback endpoint which will live until removed through the API or the service is restarted. |
| `RemoveTemporaryLoopbackEndpoint (MidiBasicLoopbackEndpointDeletionConfig)` | Remove a temporary loopback endpoint when provided the association Id Guid. |

Applications creating endpoints for app-to-app MIDI should generally use the Virtual Device support built into the API. However, applications may need to create lightweight loopback endpoints without the protocol negotiation, MIDI 2.0 discovery process, and lifetime management provided by the Virtual Device support. For those scenarios, we have a simple loopback endpoint type.

This type of loopback endpoint is comparable to existing third-party MIDI 1.0 loopback drivers in that it creates a single MIDI 1.0 Source Port and a single MIDI 1.0 Destination Port, each with the same name.

Loopback endpoints created by the user and stored in the configuration file will persist after the service is restarted or the PC rebooted. Loopback endpoints created through this API call are temporary, and will disappear if the service is restarted. In both cases, this feature requires that the basic loopback endpoint transport is installed and enabled.
