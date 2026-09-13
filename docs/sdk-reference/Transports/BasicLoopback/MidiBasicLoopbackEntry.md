---
layout: sdk_reference_page
title: MidiBasicLoopbackEntry
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: Represents an active basic MIDI 1.0-style loopback endpoint instance
---

This class represents an active transient basic loopback endpoint. Instances are returned by `MidiBasicLoopbackManager.GetActiveLoopbackEntries()` and in `MidiBasicLoopbackCreationResponse.CreatedLoopbackEntry`.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID that uniquely identifies this loopback instance |
| `EndpointDeviceId` | The full endpoint device id for this loopback endpoint |
| `Name` | The name of the loopback endpoint |
| `Description` | The description of the loopback endpoint |
| `ImageFileName` | The bare file name of the endpoint's picture within the shared endpoint assets folder, empty when there is none |
| `IsMuted` | True if this loopback is currently muted |
| `MessageCount` | Running total of UMP messages this loopback has carried from its destination back to its source since it was created. Reset when the endpoint is recreated, and not incremented while the loopback is muted |
