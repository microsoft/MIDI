---
layout: sdk_reference_page
title: MidiRuntimeArchitecture enum
namespace: Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: Provides information about the installed Windows MIDI Services App SDK Runtime. This is used primarily by the settings app.
---

## Members

| Function | Value | Description |
| --------------- | ----------- |
| `Unknown` | 0 | Unknown release architecture. This represents an error. |
| `x64` | 1 | The installed version is an Intel/AMD x86-64 version |
| `Arm64X` | 2 | The installed version is the combined Arm64EC/Arm64 classic binary. |
