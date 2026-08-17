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
| `MidiBasicLoopbackEndpointDefinition(name)` | Create a definition with the specified name. The unique id is generated for you |
| `MidiBasicLoopbackEndpointDefinition(name, description)` | Create a definition with the specified name and description. The unique id is generated for you |
| `MidiBasicLoopbackEndpointDefinition(name, description, uniqueId)` | Create a definition with the specified name, description, and your own unique id |

## Properties

| Property | Description |
|---|---|
| `Name` | The name of the endpoint. Cleaned and shortened to the specification limit, which is a UTF-8 byte count rather than a character count, so a name using non-ASCII characters may be shortened sooner than expected. |
| `UniqueId` | A short unique identifier for this endpoint, used when building the endpoint id. Invalid characters are removed and the value is limited to `MIDI_MAX_UMP_ENDPOINT_UNIQUE_ID_CHARACTER_COUNT` characters. If left empty, one is generated from the association id when the configuration is created. If, when combined with the generated loopback prefix, this id is not unique among all loopback endpoints, endpoint creation will fail. |
| `Description` | Optional description for the endpoint |
