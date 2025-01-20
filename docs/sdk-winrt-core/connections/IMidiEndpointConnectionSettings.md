---
layout: api_page
title: IMidiEndpointConnectionSettings
parent: Connections
grand_parent: Midi2 core
has_children: false
---

# IMidiEndpointConnectionSettings

Settings which are optionally provided when connecting to an endpoint. When needed, the implementation of the endpoint will come with a concrete settings class which implements this interface, and translates the settings into JSON which is sent up to the service and read by the abstraction.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Property | Description |
| -------- | ----------- |
| `SettingsJson` | The JSON string representation of the settings. Any implementation of this class must provide a valid JSON string from this property. |

## IDL

[IMidiEndpointConnectionSettings IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/IMidiEndpointConnectionSettings.idl)

