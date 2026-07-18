---
layout: sdk_reference_page
title: MidiBasicLoopbackManager
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: The primary class used to create or remove basic MIDI 1.0-style loopback endpoints
---

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | Returns true if this transport is available in the service. |
| `TransportId` | Returns the GUID of this transport. |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `CreateTransientLoopback(creationConfig)` | Create a transient loopback endpoint which will live until removed or the service is restarted. Returns a `MidiBasicLoopbackCreationResponse`. |
| `RemoveTransientLoopback(removalConfig)` | Remove a transient loopback endpoint. Returns a `MidiBasicLoopbackRemovalResponse`. |
| `GetAssociationId(basicLoopbackEndpoint)` | Returns the association GUID for the given basic loopback endpoint. |
| `DoesLoopbackExist(uniqueIdentifier)` | Returns true if a basic loopback with the specified unique identifier already exists. |
| `MuteLoopback(associationId)` | Mutes (suppresses all messages for) the loopback with the given association id. Returns a `MidiBasicLoopbackUpdateResponse`. |
| `UnmuteLoopback(associationId)` | Unmutes the loopback with the given association id. Returns a `MidiBasicLoopbackUpdateResponse`. |
| `GetActiveLoopbackEntries()` | Returns a collection of all active `MidiBasicLoopbackEntry` objects. |

Applications creating endpoints for app-to-app MIDI should generally use the Virtual Device support built into the API. However, applications may need to create lightweight loopback endpoints without the protocol negotiation, MIDI 2.0 discovery process, and lifetime management provided by the Virtual Device support. For those scenarios, we have a simple loopback endpoint type.

Loopback endpoints created by the user and stored in the configuration file will persist after the service is restarted or the PC rebooted. Loopback endpoints created through this API call are temporary, and will disappear if the service is restarted. In both cases, this feature requires that the basic loopback endpoint transport is installed and enabled.
