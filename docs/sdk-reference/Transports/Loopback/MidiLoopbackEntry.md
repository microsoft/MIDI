---
layout: sdk_reference_page
title: MidiLoopbackEntry
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: Represents an active loopback endpoint pair instance
---

This class represents an active transient loopback endpoint pair. Instances are returned by `MidiLoopbackManager.GetActiveLoopbackEntries()` and in `MidiLoopbackCreationResponse.CreatedLoopbackEntry`.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID that uniquely identifies this loopback pair |
| `EndpointA` | A `MidiLoopbackEndpointEntry` with information about the A-side endpoint |
| `EndpointB` | A `MidiLoopbackEndpointEntry` with information about the B-side endpoint |
| `IsMuted` | True if this loopback pair is currently muted |
