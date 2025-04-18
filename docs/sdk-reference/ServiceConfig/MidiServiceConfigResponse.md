---
layout: sdk_reference_page
title: MidiServiceConfigResponse
namespace: Microsoft.Windows.Devices.Midi2.ServiceConfig
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Response from the service from a configuration attempt
---

Plugin-specific configuration return results.

> <h3>Important Note</h3>
> The json config sent to and received from the service is an implementation detail, not a contract, and is subject to change. Do not attempt to manually manipulate or create json to send to the MIDI Service, or manually parse the json return results, unless you are creating a transport yourself.

## Properties

| Property | Description |
| --- | --- |
| `Status` | Indicates success or failure for the config. |
| `ResponseJson` | Json with more details. This is transport-specific. |
