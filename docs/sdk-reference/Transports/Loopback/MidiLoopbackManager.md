---
layout: sdk_reference_page
title: MidiLoopbackManager
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: The primary class used to create or remove loopback endpoints
---

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | Returns true if this transport is available in the service. |
| `TransportId` | Returns the GUID of this transport. |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `CreateTransientLoopback(creationConfig)` | Create a transient pair of loopback endpoints which will live until removed or the service is restarted. Returns a `MidiLoopbackCreationResponse`. |
| `RemoveTransientLoopback(removalConfig)` | Remove a pair of transient loopback endpoints. Returns a `MidiLoopbackRemovalResponse`. |
| `GetAssociatedLoopbackEndpointForId(loopbackEndpointId)` | Returns the `MidiEndpointDeviceInformation` for the other endpoint in a loopback pair, given a loopback endpoint device id. |
| `GetAssociatedLoopbackEndpoint(loopbackEndpoint, endpointsToSearch)` | Returns the associated endpoint in a loopback pair, searching the provided endpoint collection. |
| `GetAssociatedLoopbackEndpoint(loopbackEndpoint)` | Returns the associated endpoint in a loopback pair by searching all current endpoints. |
| `GetAssociationId(loopbackEndpoint)` | Returns the association GUID for the given loopback endpoint. |
| `DoesLoopbackAExist(uniqueIdentifier)` | Returns true if the A-side of a loopback with the specified unique identifier already exists. |
| `DoesLoopbackBExist(uniqueIdentifier)` | Returns true if the B-side of a loopback with the specified unique identifier already exists. |
| `MuteLoopback(associationId)` | Mutes the loopback pair with the given association id. Returns a `MidiLoopbackUpdateResponse`. |
| `UnmuteLoopback(associationId)` | Unmutes the loopback pair with the given association id. Returns a `MidiLoopbackUpdateResponse`. |
| `GetActiveLoopbackEntries()` | Returns a collection of all active `MidiLoopbackEntry` objects. |

Applications creating endpoints for app-to-app MIDI should generally use the Virtual Device support built into the API. However, applications may need to create lightweight loopback endpoints without the protocol negotiation, MIDI 2.0 discovery process, and lifetime management provided by the Virtual Device support. For those scenarios, we have a simple loopback endpoint type.

Loopback endpoints created by the user and stored in the configuration file will persist after the service is restarted or the PC rebooted. Loopback endpoints created through this API call are temporary, and will disappear if the service is restarted. In both cases, this feature requires that the loopback endpoint transport is installed and enabled.
