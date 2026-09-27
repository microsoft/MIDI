---
layout: sdk_reference_page
title: MidiEndpointDevicePurpose
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: Indicates the intended use of an endpoint
---

Says what an endpoint is for. Use it to sort the endpoints you show people into groups. The API also uses it when it filters endpoints with `MidiEndpointDeviceInformationFilters`.

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `NormalMessageEndpoint` | `0x00000000` | A normal endpoint for sending and receiving messages |
| `VirtualDeviceResponder` | `0x00000064` | The device side of an app-to-app MIDI connection. Only the application that hosts the device should use it |
| `InBoxGeneralMidiSynth` | `0x00000190` | The built-in General MIDI synthesizer |
| `DiagnosticLoopback` | `0x000001F4` | One of the two built-in diagnostic loopback endpoints. Applications don't normally use these |
| `DiagnosticPing` | `0x000001FE` | The internal diagnostic ping endpoint. Applications should never use it, because it's only for `MidiDiagnostics.PingService` |
