---
layout: sdk_reference_page
title: MidiDeclaredStreamConfiguration
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: Configuration information supplied by the endpoint.
---

This information is populated by the Windows Service during the MIDI 2.0 endpoint protocol negotiation process.

## Struct Fields

| Field | Description |
| --------------- | ----------- |
| `Protocol` | The agreed upon [MIDI protocol](MidiProtocolEnum.md) |
| `ReceiveJitterReductionTimestamps` | True if the endpoint is configured to receive JR timestamps |
| `SendJitterReductionTimestamps` | True if the endpoint is configured to send JR timestamps |
