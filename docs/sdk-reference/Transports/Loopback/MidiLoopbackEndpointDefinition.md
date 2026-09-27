---
layout: sdk_reference_page
title: MidiLoopbackEndpointDefinition
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: The information supplied when creating a loopback endpoint pair
---

The name and other details for one side of a loopback endpoint pair you want to create.

## Constructors

| Constructor | Description |
| --- | --- |
| `MidiLoopbackEndpointDefinition()` | Creates an empty definition |
| `MidiLoopbackEndpointDefinition(name)` | Creates a definition with this name. The unique id is made for you |
| `MidiLoopbackEndpointDefinition(name, description)` | Creates a definition with this name and description. The unique id is made for you |
| `MidiLoopbackEndpointDefinition(name, description, uniqueId)` | Creates a definition with this name, description, and your own unique id |

## Properties

| Property | Description |
|---|---|
| `Name` | The endpoint's name. It's cleaned up and shortened to the MIDI specification's limit. That limit counts UTF-8 bytes, not characters, so a name with non-ASCII characters, such as accented letters, may be shortened sooner than you expect |
| `UniqueId` | A short unique id for this endpoint, used to build the endpoint device id. Characters that aren't allowed are removed, and it's limited to `MIDI_MAX_UMP_ENDPOINT_UNIQUE_ID_CHARACTER_COUNT` characters. If you leave it empty, one is made from the association id when the configuration is created. The endpoint can't be created if another loopback endpoint on the same side (A or B) already uses this id |
| `Description` | An optional description for the endpoint |
| `ImageFileName` | An optional file name of a picture in the shared endpoint assets folder, used as the endpoint's icon. It's a file name, not a path. If you supply a path, only the file name is kept. Your app has to copy the picture into the assets folder first |
| `CreateOnlyUmpEndpoint` | When true, only the UMP endpoint is created. When false, which is the default, MIDI 1.0 ports are created with it for older apps |
