---
layout: api_page
title: MidiLoopbackEndpointManager
parent: Midi2.Endpoints.Loopback
has_children: false
---

# MidiLoopbackEndpointManager

## Location

| Namespace | Microsoft.Windows.Devices.Midi2.Endpoints.Loopback |
| Library | Microsoft.Windows.Devices.Midi2 |

## Static Properties

| Function | Description |
| -------- | ----------- |
| `IsTransportAvailable` | Returns true if this transport is available in the service. |
| `TransportId` | Returns the GUID of this transport. |

## Static Functions

| Function | Description |
| -------- | ----------- |
| `CreateTransientLoopbackEndpoints(MidiLoopbackEndpointCreationConfig)` | Create a pair of loopback endpoints which will live until removed through the API or the service is restarted. |
| `RemoveTemporaryLoopbackEndpoints(MidiLoopbackEndpointDeletionConfig)` | Remove a pair of temporary loopback endpoints when provided their association Id Guid. |

Applications creating endpoints for app-to-app MIDI should generally use the Virtual Device support built into the API. However, applications may need to create lightweight loopback endpoints without the protocol negotiation, MIDI 2.0 discovery process, and lifetime management provided by the Virtual Device support. For those scenarios, we have a simple loopback endpoint type.

Loopback endpoints created by the user and stored in the configuration file will persist after the service is restarted or the PC rebooted. Loopback endpoints created through this API call are temporary, and will disappear if the service is restarted. In both cases, this feature requires that the loopback endpoint transport is installed and enabled.

## IDL

[MidiLoopbackEndpointManager IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiLoopbackEndpointManager.idl)