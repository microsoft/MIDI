---
layout: api_page
title: MidiServiceLoopbackEndpointDefinition
parent: Microsoft.Devices.Midi2.Endpoints.Loopback
has_children: false
---

# MidiServiceLoopbackEndpointDefinition

This class defines the properties of an endpoint which can be created at runtime. For example, a loopback endpoint.

## Properties

| Property | Description |
|---|---|
| `Name` | The name of the endpoint. |
| `UniqueId` | A short unique identifier for this endpoint. This is used in creating the id. Keep to 32 characters or fewer (32 characters is the length of a no-symbols GUID). If, when combined with the generated loopback A/B differentiator prefix, this id is not unique among all loopback endpoints, endpoint creation will fail. |
| `Description` | Optional description for the endpoint |

## IDL

[MidiServiceLoopbackEndpointDefinition IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiServiceLoopbackEndpointDefinition.idl)

