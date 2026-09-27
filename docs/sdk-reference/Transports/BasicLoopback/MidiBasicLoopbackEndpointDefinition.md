---
layout: sdk_reference_page
title: MidiBasicLoopbackEndpointDefinition
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
description: The information supplied when creating a basic MIDI 1.0-style loopback endpoint
---

The name and other details for a basic loopback endpoint you want to create.

## Constructors

| Constructor | Description |
| --- | --- |
| `MidiBasicLoopbackEndpointDefinition()` | Creates an empty definition |
| `MidiBasicLoopbackEndpointDefinition(name)` | Creates a definition with this name. The unique id is made for you |
| `MidiBasicLoopbackEndpointDefinition(name, description)` | Creates a definition with this name and description. The unique id is made for you |
| `MidiBasicLoopbackEndpointDefinition(name, description, uniqueId)` | Creates a definition with this name, description, and your own unique id |

## Properties

| Property | Description |
|---|---|
| `Name` | The endpoint's name. It's cleaned up and shortened to the MIDI specification's limit. That limit counts UTF-8 bytes, not characters, so a name with non-ASCII characters, such as accented letters, may be shortened sooner than you expect |
| `UniqueId` | A short unique id for this endpoint, used to build the endpoint device id. Characters that aren't allowed are removed, and it's limited to `MIDI_MAX_UMP_ENDPOINT_UNIQUE_ID_CHARACTER_COUNT` characters. If you leave it empty, one is made from the association id when the configuration is created. The endpoint can't be created if this id, together with the prefix the transport adds, is already used by another loopback endpoint |
| `Description` | An optional description for the endpoint |
| `ImageFileName` | The file name, with no folder, of a picture for this endpoint in the shared endpoint assets folder. Your app copies the picture the person chose into that folder, and puts its name here |
