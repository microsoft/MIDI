---
layout: api_page
title: MidiVirtualDeviceCreationConfig
parent: Midi2.Endpoints.Virtual
has_children: false
---

# MidiVirtualDeviceCreationConfig

The `MidiVirtualDeviceCreationConfig` class specifies, in an easy to use format, the responses for endpoint discovery as well as the properties to use when constructing the device endpoint.

## Properties

| Property | Description |
| --------------- | ----------- |
| `Name` | The transport-supplied name to use for this device |
| `Description` | The transport-supplied description to use for this device |
| `Manufacturer` | The transport-supplied manufacturer name to use for this device |
| `DeclaredDeviceIdentity` | The `MidiDeclaredDeviceIdentity` to use for responding to MIDI 2.0 endpoint discovery |
| `DeclaredEndpointInfo` | The `MidiDeclaredEndpointInfo` to use for responding to MIDI 2.0 endpoint discovery |
| `UserSuppliedInfo` | Any user-supplied information for this endpoint  |
| `FunctionBlocks` | The set of function blocks to be declared for this endpoint |

## Sample

[Full Example](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/app-to-app-midi-cs)

## IDL

[MidiVirtualDeviceCreationConfig IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-endpoints-virtual/MidiVirtualDeviceCreationConfig.idl)
