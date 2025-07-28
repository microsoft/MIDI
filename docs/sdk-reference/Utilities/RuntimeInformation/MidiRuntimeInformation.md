---
layout: sdk_reference_page
title: MidiRuntimeInformation
namespace: Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Provides information about the installed Windows MIDI Services App SDK Runtime. This is used primarily by the settings app.
---


## Static Functions

| Function | Description |
| --------------- | ----------- |
| `GetInstalledVersion` | Returns the `MidiRuntimeVersion` representing the installed SDK |
| `GetInstalledArchitecture` | Returns a `MidiRuntimeArchitecture` (Arm64X, x64) for what is installed |
| `GetInstalledReleaseType` | Returns a single member of `MidiRuntimeReleaseTypes` which identifies if the installed version is a preview, stable, or other release. |
