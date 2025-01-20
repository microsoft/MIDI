---
layout: api_page
title: MidiLoopbackEndpointCreationConfig
parent: Midi2.Endpoints.Loopback
has_children: false
---

# MidiLoopbackEndpointCreationConfig

The configuration sent to the service when an application wants to create a loopback endpoint pair

## Location

| Namespace | Microsoft.Windows.Devices.Midi2.Endpoints.Loopback |
| Library | Microsoft.Windows.Devices.Midi2 |

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
| `MidiLoopbackEndpointCreationConfig(associationId, endpointDefinitionA, endpointDefinitionB)` | Create a configuration with the specified associationId and endpoint definitions |


## IDL

[MidiLoopbackEndpointCreationConfig IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiLoopbackEndpointCreationConfig.idl)