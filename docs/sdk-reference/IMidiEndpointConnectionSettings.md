---
layout: sdk_reference_page
title: IMidiEndpointConnectionSettings
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: interface
description: Settings which are optionally provided when connecting to an endpoint
---

Settings which are optionally provided when connecting to an endpoint. When needed, the implementation of the endpoint will come with a concrete settings class which implements this interface, and translates the settings into JSON which is sent up to the service and read by the abstraction.

## Properties

| Property | Description |
| -------- | ----------- |
| `SettingsJson` | The JSON string representation of the settings. Any implementation of this class must provide a valid JSON string from this property. |

