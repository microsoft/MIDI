---
layout: sdk_reference_page
title: MidiServiceConfigResponseStatus
namespace: Microsoft.Windows.Devices.Midi2.ServiceConfig
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: Indicates success or failure for a configuration attempt.
---

Enum which indicates if a configuration attempt resulted in success or failure.

> <h3>Important Note</h3>
> The json config sent to and received from the service is an implementation detail, not a contract, and is subject to change. Do not attempt to manually manipulate or create json to send to the MIDI Service, or manually parse the json return results, unless you are creating a transport yourself.

## Properties

| Property | Value | Description |
| --- | --- | --- |
| `Success` | 0 | The call succeeded |
| `ErrorTargetNotFound` | 404 | The target transport or transform was not found |
| `ErrorConfigJsonNullOrEmpty` | 600 | The supplied config json is missing |
| `ErrorProcessingConfigJson` | 601 | There's an error in the config json |
| `ErrorProcessingResponseJson` | 605 | There's an error in the response json |
| `ErrorNotImplemented` | 2600 | The requested capability is not implemented |
