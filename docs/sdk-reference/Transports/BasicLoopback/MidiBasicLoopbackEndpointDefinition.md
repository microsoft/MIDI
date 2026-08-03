---
layout: sdk_reference_page
title: MidiBasicLoopbackEndpointDefinition
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: The information supplied when creating a basic MIDI 1.0-style loopback endpoint
---

## Constructors

| Constructor | Description |
| --- | --- |
| `MidiBasicLoopbackEndpointDefinition()` | Create an empty definition |
| `MidiBasicLoopbackEndpointDefinition(name, uniqueId)` | Create a definition with the specified name and unique id |
| `MidiBasicLoopbackEndpointDefinition(name, uniqueId, description)` | Create a definition with the specified name, unique id, and description |

## Properties

| Property | Description |
|---|---|
| `Name` | The name of the endpoint. |
| `UniqueId` | A short unique identifier for this endpoint. This is used in creating the id. Keep to 32 characters or fewer (32 characters is the length of a no-symbols GUID). If, when combined with the generated loopback prefix, this id is not unique among all loopback endpoints, endpoint creation will fail. |
| `Description` | Optional description for the endpoint |
