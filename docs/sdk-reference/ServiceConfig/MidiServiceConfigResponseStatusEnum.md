---
layout: sdk_reference_page
title: MidiServiceConfigResponseStatus
namespace: Windows.Devices.Midi2.ServiceConfig
type: enum
description: Indicates success or failure for a configuration attempt.
---

Says whether sending a configuration to the service worked, and if not, why.

> **Important:** The JSON that goes to and from the service is an implementation detail, not a contract, and it can change. Don't build or edit this JSON by hand, and don't parse what comes back, unless you're writing a transport yourself.

## Properties

| Property | Value | Description |
| --- | --- | --- |
| `Success` | `0x00000000` | It worked |
| `ErrorTargetNotFound` | `0x00000194` | The service couldn't find the transport or transform |
| `ErrorConfigJsonNullOrEmpty` | `0x00000258` | The configuration JSON is missing |
| `ErrorProcessingConfigJson` | `0x00000259` | There's an error in the configuration JSON |
| `ErrorProcessingResponseJson` | `0x0000025D` | There's an error in the JSON that came back |
| `ErrorFromService` | `0x00000328` | The service returned an error |
| `ErrorNotImplemented` | `0x00000A28` | The transport can't do what was asked |
