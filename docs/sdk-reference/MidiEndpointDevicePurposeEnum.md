---
layout: sdk_reference_page
title: MidiEndpointDevicePurpose
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: Indicates the intended use of an endpoint
---

Indicates the intended purpose of the endpoint. Use this to help classify endpoints you show to users in your application. This value is also used internally when filtering endpoints per the `MidiEndpointDeviceInformationFilters` enumeration.

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `NormalMessageEndpoint` | `0` | The endpoint is any number of normal messaging endpoint types. |
| `VirtualDeviceResponder` | `100` | The endpoint is the device-side of an app-to-app MIDI connection. Only the device app should use this endpoint. |
| `InBoxGeneralMidiSynth` | `400` | The endpoint represents the internal General MIDI Synthesizer  |
| `DiagnosticLoopback` | `500` | The endpoint is one of the static system-wide diagnostics loopback endpoints. These are not normally used in applications  |
| `DiagnosticPing` | `510` | The endpoint is the internal diagnostics ping endpoint. This endpoint should never be used by applications as it is reserved for the `MidiService` ping feature.  |
