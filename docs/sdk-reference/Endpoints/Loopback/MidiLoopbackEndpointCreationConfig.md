---
layout: sdk_reference_page
title: MidiLoopbackEndpointCreationConfig
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Loopback
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Config sent to the service to create a loopback endpoint pair
---

This is the configuration sent to the service when an application wants to create a loopback endpoint pair.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID used to uniquely identify this endpoint pair |
| `EndpointDefinitionA` | `MidiLoopbackEndpointDefinition` for the A-side of the pair |
| `EndpointDefinitionB` | `MidiLoopbackEndpointDefinition` for the B-side of the pair |

## Methods

| Name | Description |
| -------- | ----------- |
| `MidiLoopbackEndpointCreationConfig` | Create an empty config |
| `MidiLoopbackEndpointCreationConfig (associationId, endpointDefinitionA, endpointDefinitionB)` | Create a configuration with the specified associationId and endpoint definitions |
