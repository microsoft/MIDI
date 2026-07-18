---
layout: sdk_reference_page
title: MidiApiMode
namespace: Windows.Devices.Midi2
type: enum
description: Indicates the currently active MIDI API mode
---

The `MidiApiMode` enumeration indicates which MIDI API mode the system is currently operating in. This is returned by `MidiApi.GetCurrentlySelectedApiMode()`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `FullWindowsMidiServicesMode` | `0x00000000` | The system is running fully with Windows MIDI Services. All MIDI 1.0 and MIDI 2.0 features are available. |
| `LegacyMode` | `0x00000001` | The system is running in legacy WinMM-only mode. Windows MIDI Services features such as multi-client or new transports, are not available. |
| `HybridLegacyMode` | `0x00000002` | The system is running in a hybrid mode where both Windows MIDI Services and legacy WinMM-style MIDI are available. However, the two are siloed so that WinMM clients do not see Windows MIDI Services endpoints using new transports or the new combined MIDI1/MIDI2 USB driver, and Windows MIDI Services SDK apps do not see any ports created from WinMM, the older usbaudio.sys MIDI1 USB driver, or vendor MIDI 1 drivers. |
