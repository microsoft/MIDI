---
layout: api_page
title: MidiServiceLoopbackEndpointCreationResult
parent: Microsoft.Devices.Midi2.Endpoints.Loopback
has_children: false
---

# MidiServiceLoopbackEndpointCreationResult

This class represents the results of an attempt to create runtime loopback endpoints

## Properties

| Property | Description |
|---|---|
| `Success` | True if the creation of both endpoints was a success |
| `AssociatioNId` | The GUID which associatiates the two endpoints. Provided during creation time. |
| `EndpointDeviceIdA` | The full endpoint device id `\\SWD\...` for the endpoint identified as the "A" side of the loopback |
| `EndpointDeviceIdB` | The full endpoint device id `\\SWD\...` for the endpoint identified as the "B" side of the loopback |

## IDL

[MidiServiceLoopbackEndpointCreationResult IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiServiceLoopbackEndpointCreationResult.idl)

