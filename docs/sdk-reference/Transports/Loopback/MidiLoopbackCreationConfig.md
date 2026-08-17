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
| `MidiLoopbackCreationConfig(endpointDefinitionA, endpointDefinitionB)` | Create a configuration with the specified endpoint definitions |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | Read-only. The GUID which uniquely identifies this loopback pair, generated when the configuration is created. Use it to remove the loopback later. |
| `EndpointDefinitionA` | `MidiLoopbackEndpointDefinition` for the A-side of the pair |
| `EndpointDefinitionB` | `MidiLoopbackEndpointDefinition` for the B-side of the pair |
| `IsMuted` | When true, the loopback endpoints are created but all messages are suppressed |

## Remarks

The association id is generated rather than supplied, because it is an internal identifier with no meaning to the user. Assigning a definition also fills in its `UniqueId` if you left that empty, so a caller who only wants to name the endpoints does not have to invent any identifiers.
