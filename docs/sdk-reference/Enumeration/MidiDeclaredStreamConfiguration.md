---
layout: sdk_reference_page
title: MidiDeclaredStreamConfiguration
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Configuration information supplied by the endpoint.
---

This information is populated by the Windows Service during the MIDI 2.0 endpoint protocol negotiation process. When retrieved from a `MidiEndpointDeviceInformation` object, it is read-only.

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if this object should be treated as read-only |
| `Protocol` | The agreed upon [MIDI protocol](MidiProtocolEnum.md) |
| `ReceiveJitterReductionTimestamps` | True if the endpoint is configured to receive JR timestamps |
| `SendJitterReductionTimestamps` | True if the endpoint is configured to send JR timestamps |
