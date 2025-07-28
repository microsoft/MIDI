---
layout: sdk_reference_page
title: MidiRuntimeReleaseTypes enum
namespace: Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: Provides information about the installed Windows MIDI Services App SDK Runtime. This is used primarily by the settings app.
---

This is a Flags enum because it's also used by the updater for checking for new versions. When used to report on what is installed, only one value will be reported.

## Members

| Function | Value | Description |
| --------------- | ----------- |
| `Unknown` | 0 | An unknown release type / channel |
| `Stable` | 1 | The installed version is a stable release |
| `Preview` | 2 | The installed version is a preview release |
