---
layout: sdk_reference_page
title: MidiBasicLoopbackCreationConfig
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to create a basic MIDI 1.0-style loopback endpoint
---

This is the configuration sent to the service when an application wants to create a transient basic loopback endpoint.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBasicLoopbackCreationConfig()` | Create an empty config |
| `MidiBasicLoopbackCreationConfig(endpointDefinition)` | Create a configuration with the specified endpoint definition |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | Read-only. The GUID which uniquely identifies this loopback, generated when the configuration is created. Use it to remove the loopback later. |
| `EndpointDefinition` | The `MidiBasicLoopbackEndpointDefinition` for this loopback |
| `IsMuted` | When true, the loopback endpoint is created but all messages are suppressed |

## Remarks

The association id is generated rather than supplied, because it is an internal identifier with no meaning to the user. Assigning the definition also fills in its `UniqueId` if you left that empty, so a caller who only wants to name the endpoint does not have to invent any identifiers.
