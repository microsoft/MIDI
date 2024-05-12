---
layout: api_page
title: IMidiEndpointConnectionSettings
parent: Connections
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# IMidiEndpointConnectionSettings

Settings which are optionally provided when connecting to an endpoint. Typically, the implementation of the endpoint will come with a concrete settings class which implements this interface, and translates the settings into JSON which is sent up to the service and read by the abstraction.

## Properties

| Property | Description |
| -------- | ----------- |
| `SettingsJson` | The JSON representation of the settings. |

## IDL

[IMidiEndpointConnectionSettings IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/IMidiEndpointConnectionSettings.idl)

