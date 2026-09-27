---
layout: sdk_reference_page
title: MidiBasicLoopbackManager
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: The primary class used to create or remove basic MIDI 1.0-style loopback endpoints
---

Creates, removes, mutes, and lists basic loopback endpoints.

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | True if this transport is available in the service |
| `TransportId` | The GUID of this transport |
| `IsFeedbackProtectionAvailable` | False when the basic loopback transport on this PC can't watch for feedback |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `CreateTransientLoopback(creationConfig)` | Creates a temporary loopback endpoint. It lasts until you remove it or the service restarts. Returns a `MidiBasicLoopbackCreationResponse` |
| `RemoveTransientLoopback(removalConfig)` | Removes a temporary loopback endpoint. Returns a `MidiBasicLoopbackRemovalResponse` |
| `GetAssociationId(basicLoopbackEndpoint)` | Returns the association GUID of this basic loopback endpoint |
| `DoesLoopbackExist(uniqueIdentifier)` | Returns true if a basic loopback with this unique id already exists |
| `MuteLoopback(associationId)` | Mutes the loopback with this association id, so no messages get through. Returns a `MidiBasicLoopbackUpdateResponse` |
| `UnmuteLoopback(associationId)` | Unmutes the loopback with this association id. Returns a `MidiBasicLoopbackUpdateResponse` |
| `SetFeedbackProtection(associationId, feedbackProtection)` | Changes what the loopback does if MIDI feeds back into it. Takes effect right away. Save the change to the configuration to keep it after a restart. Returns a `MidiBasicLoopbackUpdateResponse` |
| `GetActiveLoopbackEntries()` | Returns a `MidiBasicLoopbackEntry` for each active basic loopback |

## Remarks

If your app creates endpoints so it can talk to other apps, it should usually use the virtual device support in the API. But sometimes an app needs a simpler loopback endpoint, without the protocol negotiation, MIDI 2.0 discovery, and lifetime management that virtual devices have. That's what basic loopbacks are for.

Loopback endpoints that people create in the MIDI tools are saved in the configuration, so they stay after the service restarts or the PC reboots. Loopback endpoints created with this API are temporary, and go away when the service restarts. Either way, the basic loopback transport must be installed and turned on.
