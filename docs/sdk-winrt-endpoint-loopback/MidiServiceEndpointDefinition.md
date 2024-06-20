---
layout: api_page
title: MidiServiceLoopbackEndpointDefinition
parent: Midi2.Endpoints.Loopback
has_children: false
---

# MidiServiceLoopbackEndpointDefinition


## Properties

| Property | Description |
|---|---|
| `Name` | The name of the endpoint. |
| `UniqueId` | A short unique identifier for this endpoint. This is used in creating the id. Keep to 32 characters or fewer (32 characters is the length of a no-symbols GUID). If, when combined with the generated loopback A/B differentiator prefix, this id is not unique among all loopback endpoints, endpoint creation will fail. |
| `Description` | Optional description for the endpoint |

## IDL

[MidiServiceLoopbackEndpointDefinition IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-endpoints-loopback/MidiServiceLoopbackEndpointDefinition.idl)