---
layout: sdk_reference_page
title: Midi1PortNamingApproach
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: Indicates how MIDI 1.0 port names are generated for a UMP endpoint
---

Sets how Windows MIDI Services names the MIDI 1.0 ports it creates for a UMP endpoint. To learn how the names are made, see [How MIDI 1.0 port names are generated]({{ site.baseurl }}/kb/how-midi1-port-names-are-generated/).

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Default` | `0x00000000` | Use the default approach, which Windows chooses. |
| `UseClassicCompatible` | `0x00000001` | Use names made the way WinMM has always named MIDI ports. |
| `UseNewStyle` | `0x00000002` | Use the newer Windows MIDI Services style of port names. |
| `UseAutomatic` | `0x00000003` | Let Windows decide for this endpoint, based on what the device reported. An endpoint whose ports already had WinMM names keeps them. One with no classic equivalent gets new-style names. |
