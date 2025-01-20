---
layout: api_page
title: MidiLoopbackEndpointCreationResult
parent: Midi2.Endpoints.Loopback
has_children: false
---

# MidiLoopbackEndpointCreationResult

This class represents the results of an attempt to create runtime transient loopback endpoints

## Location

| Namespace | Microsoft.Windows.Devices.Midi2.Endpoints.Loopback |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Property | Description |
|---|---|
| `Success` | True if the creation of both endpoints was a success |
| `AssociatioNId` | The GUID which associatiates the two endpoints. Provided during creation time. |
| `EndpointDeviceIdA` | The full endpoint device id `\\swd\...` for the endpoint identified as the "A" side of the loopback |
| `EndpointDeviceIdB` | The full endpoint device id `\\swd\...` for the endpoint identified as the "B" side of the loopback |

## IDL

[MidiLoopbackEndpointCreationResult IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiLoopbackEndpointCreationResult.idl)

