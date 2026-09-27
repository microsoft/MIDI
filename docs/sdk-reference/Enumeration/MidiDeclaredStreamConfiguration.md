---
layout: sdk_reference_page
title: MidiDeclaredStreamConfiguration
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Configuration information supplied by the endpoint.
---

The MIDI service fills this in during MIDI 2.0 protocol negotiation. When you get one from `MidiEndpointDeviceInformation`, treat it as read-only. Changing it doesn't change the endpoint.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiDeclaredStreamConfiguration()` | Creates an empty object |
| `MidiDeclaredStreamConfiguration(protocol, receiveJitterReductionTimestamps, sendJitterReductionTimestamps)` | Creates an object with these values |

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if you should treat this object as read-only |
| `Protocol` | The [MIDI protocol]({{ site.baseurl }}/sdk-reference/Enumeration/MidiProtocolEnum/) the service and the endpoint agreed on |
| `ReceiveJitterReductionTimestamps` | True if the endpoint is set up to receive jitter reduction (JR) timestamps |
| `SendJitterReductionTimestamps` | True if the endpoint is set up to send jitter reduction timestamps |
