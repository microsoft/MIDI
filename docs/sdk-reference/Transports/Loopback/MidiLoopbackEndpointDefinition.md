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
| `ImageFileName` | Optional bare file name of a picture in the shared endpoint assets folder, used as the icon for the endpoint. This is a file name, not a path: any path supplied here is reduced to its file name. The app is responsible for copying the picture into the assets folder first. |
| `CreateOnlyUmpEndpoint` | When true, only the UMP endpoint is created. When false, the default, MIDI 1.0 ports are created alongside it for older apps. |
