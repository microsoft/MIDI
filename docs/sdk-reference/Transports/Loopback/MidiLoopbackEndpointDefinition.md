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
| `MidiLoopbackEndpointDefinition(name)` | Create a definition with the specified name. The unique id is generated for you |
| `MidiLoopbackEndpointDefinition(name, description)` | Create a definition with the specified name and description. The unique id is generated for you |
| `MidiLoopbackEndpointDefinition(name, description, uniqueId)` | Create a definition with the specified name, description, and your own unique id |

## Properties

| Property | Description |
|---|---|
| `Name` | The name of the endpoint. Cleaned and shortened to the specification limit, which is a UTF-8 byte count rather than a character count, so a name using non-ASCII characters may be shortened sooner than expected. |
| `UniqueId` | A short unique identifier for this endpoint, used when building the endpoint id. Invalid characters are removed and the value is limited to `MIDI_MAX_UMP_ENDPOINT_UNIQUE_ID_CHARACTER_COUNT` characters. If left empty, one is generated from the association id when the configuration is created. If the id is not unique among all loopback endpoints for this side (A or B), endpoint creation will fail. |
| `Description` | Optional description for the endpoint |
