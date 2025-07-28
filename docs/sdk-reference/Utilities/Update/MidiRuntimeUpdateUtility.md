---
layout: sdk_reference_page
title: MidiRuntimeUpdateUtility
namespace: Microsoft.Windows.Devices.Midi2.Utilities.Update
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Used primarily by the MIDI Settings app to check for new SDK runtime releases
---

## Static Functions

| Function | Description |
| --------------- | ----------- |
| `GetAllAvailableReleases` | Makes a web request and returns the available releases. |
| `GetHighestAvailableRelease` | Makes a web request and returns the highest/latest available release which matches the filter criteria |
| `IsReleaseNewerThanInstalled` | Returns true if the available release is newer than the currently installed release |
