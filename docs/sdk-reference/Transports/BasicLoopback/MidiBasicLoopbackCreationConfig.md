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
| `MidiBasicLoopbackCreationConfig(associationId, endpointDefinition)` | Create a configuration with the specified association id and endpoint definition |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID used to uniquely identify this loopback |
| `EndpointDefinition` | The `MidiBasicLoopbackEndpointDefinition` for this loopback |
| `IsMuted` | When true, the loopback endpoint is created but all messages are suppressed |
