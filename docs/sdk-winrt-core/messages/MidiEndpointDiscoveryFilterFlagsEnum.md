---
layout: api_page
title: MidiEndpointDiscoveryFilterFlags
parent: Messages
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiEndpointDiscoveryFilterFlags Enumeration

Used to indicate which endpoint discovery messages you want to receive when you query an endpoint.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `None` | `0x00000000` | Request nothing. |
| `RequestEndpointInformation` | `0x00000001` | Request the details of the endpoint. |
| `RequestDeviceIdentity` | `0x00000002` | Request identity information including System Exclusive Ids and version information |
| `RequestEndpointName` | `0x00000004` | Request endpoint name messages |
| `RequestProductInstanceId` | `0x00000008` | Request product instance id messages |
| `RequestStreamConfiguration` | `0x00000010` | Request the stream configuration |


## IDL

[MidiEndpointDiscoveryFilterFlagsEnum IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiEndpointDiscoveryFilterFlagsEnum.idl)
