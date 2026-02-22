---
layout: sdk_reference_page
title: MidiBasicLoopbackEndpointCreationConfig
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Config sent to the service to create a basic MIDI 1.0 loopback endpoint
---

This is the configuration sent to the service when an application wants to create a loopback endpoint pair.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID used to uniquely identify this endpoint pair |
| `EndpointDefinition` | `MidiBasicLoopbackEndpointDefinition` |

## Methods

| Name | Description |
| -------- | ----------- |
| `MidiBasicLoopbackEndpointCreationConfig` | Create an empty config |
| `MidiBasicLoopbackEndpointCreationConfig (associationId, endpointDefinition)` | Create a configuration with the specified associationId and endpoint definition |
