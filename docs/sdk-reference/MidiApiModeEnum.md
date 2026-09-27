---
layout: sdk_reference_page
title: MidiApiMode
namespace: Windows.Devices.Midi2
type: enum
description: Indicates the currently active MIDI API mode
---

`MidiApi.GetCurrentlySelectedApiMode()` returns this value. It says which MIDI API mode this PC is set to use. To learn how the mode is changed, see [How to change the API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/).

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `FullWindowsMidiServicesMode` | `0x00000000` | Windows MIDI Services handles all MIDI on this PC, and every MIDI 1.0 and MIDI 2.0 feature is available. This is the normal mode |
| `LegacyMode` | `0x00000001` | The PC uses only the older WinMM MIDI system. Windows MIDI Services features, such as sharing a device between applications and the new transports, aren't available |
| `HybridLegacyMode` | `0x00000002` | Windows MIDI Services and the older WinMM MIDI system both run, but they're kept apart. WinMM applications don't see Windows MIDI Services endpoints that use the new transports or the new combined MIDI 1.0 and MIDI 2.0 USB driver. Applications that use the Windows MIDI Services WinRT API don't see ports that come from WinMM, from the older `usbaudio.sys` MIDI 1.0 USB driver, or from vendor MIDI 1.0 drivers |
