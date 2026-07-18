---
layout: sdk_reference_page
title: MidiLoopbackCreationConfig
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to create a loopback endpoint pair
---

This is the configuration sent to the service when an application wants to create a transient loopback endpoint pair.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiLoopbackCreationConfig()` | Create an empty config |
| `MidiLoopbackCreationConfig(associationId, endpointDefinitionA, endpointDefinitionB)` | Create a configuration with the specified association id and endpoint definitions |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID used to uniquely identify this loopback pair |
| `EndpointDefinitionA` | `MidiLoopbackEndpointDefinition` for the A-side of the pair |
| `EndpointDefinitionB` | `MidiLoopbackEndpointDefinition` for the B-side of the pair |
| `IsMuted` | When true, the loopback endpoints are created but all messages are suppressed |
