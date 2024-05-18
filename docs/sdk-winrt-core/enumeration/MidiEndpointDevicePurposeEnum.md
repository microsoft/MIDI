---
layout: api_page
title: MidiEndpointDevicePurpose
parent: Endpoint Enumeration
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiEndpointDevicePurpose Enumeration

Indicates the intended purpose of the endpoint. Use this to help classify endpoints you show to users in your application. This value is also used internally when filtering endpoints per the `MidiEndpointDeviceInformationFilter` enumeration.

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `NormalMessageEndpoint` | `0` | The endpoint is any number of normal messaging endpoint types. |
| `VirtualDeviceResponder` | `100` | The endpoint is the device-side of an app-to-app MIDI connection. Only the device app should use this endpoint. |
| `InBoxGeneralMidiSynth` | `400` | The endpoint represents the internal General MIDI Synthesizer  |
| `DiagnosticLoopback` | `500` | The endpoint is one of the static system-wide diagnostics loopback endpoints. These are not normally used in applications  |
| `DiagnosticPing` | `510` | The endpoint is the internal diagnostics ping endpoint. This endpoint should never be used by applications as it is reserved for the `MidiService` ping feature.  |

## IDL

[MidiEndpointDevicePurposeEnum IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiEndpointDevicePurposeEnum.idl)

