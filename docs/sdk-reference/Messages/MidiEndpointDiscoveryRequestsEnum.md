---
layout: sdk_reference_page
title: MidiEndpointDiscoveryRequests
namespace: Microsoft.Windows.Devices.Midi2.Messages
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: MIDI 2.0 endpoint discovery request flags
---

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
