---
layout: sdk_reference_page
title: Midi1PortNamingApproach
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: Indicates how MIDI 1.0 port names are generated for a UMP endpoint
---

Controls how MIDI 1.0 API port names are generated when Windows MIDI Services creates legacy-compatible ports for a UMP endpoint.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Default` | `0x00000000` | Use the default naming approach as determined by the system. |
| `UseClassicCompatible` | `0x00000001` | Use names compatible with the classic WinMM MIDI port naming scheme. |
| `UseNewStyle` | `0x00000002` | Use the new Windows MIDI Services style port names. |
