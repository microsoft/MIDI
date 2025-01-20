---
layout: api_page
title: MidiLoopbackEndpointDefinition
parent: Midi2.Endpoints.Loopback
has_children: false
---

# MidiLoopbackEndpointDefinition

## Location

| Namespace | Microsoft.Windows.Devices.Midi2.Endpoints.Loopback |
| Library | Microsoft.Windows.Devices.Midi2 |

## Struct Properties

| Property | Description |
|---|---|
| `Name` | The name of the endpoint. |
| `UniqueId` | A short unique identifier for this endpoint. This is used in creating the id. Keep to 32 characters or fewer (32 characters is the length of a no-symbols GUID). If, when combined with the generated loopback A/B differentiator prefix, this id is not unique among all loopback endpoints, endpoint creation will fail. |
| `Description` | Optional description for the endpoint |

## IDL

[MidiLoopbackEndpointDefinition IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiLoopbackEndpointDefinition.idl)