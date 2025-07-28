---
layout: sdk_reference_page
title: MidiRuntimeRelease
namespace: Microsoft.Windows.Devices.Midi2.Utilities.Update
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Represents a single available MIDI SDK runtime release
---

## Properties

| Function | Description |
| --------------- | ----------- |
| `Type` | The type of release (Preview or Stable) |
| `Source` | The source of this release. |
| `Name` | The name of this release |
| `Description` | The description of this release |
| `BuildDate` | Provides the date the build was generated |
| `Version` | Returns a `MidiRuntimeVersion` associated with this release  |
| `ReleaseNotesUri` | The URL of the release notes for this release |
| `DirectDownloadUriX64` | The URL to download the Intel/AMD x64 installer |
| `DirectDownloadUriArm64` | The URL to download the Arm64 installer |
| `DirectDownloadUriForCurrentRuntimeArchitecture` | Provides the appropriate URL for the architecture the current SDK was compiled to. This will be one of the above URLs. |
