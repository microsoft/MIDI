---
layout: sdk_reference_page
title: MidiLoopbackEndpointDefinition
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: The information supplied when creating a loopback endpoint pair
---

## Constructors

| Constructor | Description |
| --- | --- |
| `MidiLoopbackEndpointDefinition()` | Create an empty definition |
| `MidiLoopbackEndpointDefinition(name, uniqueId)` | Create a definition with the specified name and unique id |
| `MidiLoopbackEndpointDefinition(name, uniqueId, description)` | Create a definition with the specified name, unique id, and description |

## Properties

| Property | Description |
|---|---|
| `Name` | The name of the endpoint. |
| `UniqueId` | A short unique identifier for this endpoint. This is used in creating the id. Keep to 32 characters or fewer. If this id is not unique among all loopback endpoints for this side (A or B), endpoint creation will fail. |
| `Description` | Optional description for the endpoint |
