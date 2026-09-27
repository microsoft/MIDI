---
layout: sdk_reference_page
title: MidiEndpointDiscoveryRequests
namespace: Windows.Devices.Midi2.Utilities.Messages
type: enum
description: MIDI 2.0 endpoint discovery request flags
---

Says which endpoint discovery messages you want back when you ask an endpoint about itself. This is a flags enumeration, so you can combine values.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `None` | `0x00000000` | Ask for nothing |
| `RequestEndpointInfo` | `0x00000001` | Ask for the endpoint's details |
| `RequestDeviceIdentity` | `0x00000002` | Ask for identity information, including System Exclusive ids and version information |
| `RequestEndpointName` | `0x00000004` | Ask for the endpoint name messages |
| `RequestProductInstanceId` | `0x00000008` | Ask for the product instance id messages |
| `RequestStreamConfiguration` | `0x00000010` | Ask for the stream configuration |
