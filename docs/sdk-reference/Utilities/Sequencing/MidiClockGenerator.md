---
layout: sdk_reference_page
title: MidiClockGenerator
namespace: Microsoft.Windows.Devices.Midi2.Utilities.Sequencing
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Provides a simple MIDI Beat Clock generator
status: preview
---

The beat clock generator is currently experimental. With internal software endpoints, the accuracy is great and jitter is in the microsecond (fraction of a millisecond) range. With some USB devices, it's higher.

The clock uses the service's built-in message scheduler, and schedules a few seconds worth of messages at a time. The exact number of seconds is an implementation detail and so could change (view the code to see the current value).

## Functions

| Function | Description |
| --------------- | ----------- |
| `MidiClockGenerator` | Construct a clock generator with a list of destinations (endpoints and groups), a tempo BPM, and a resolution. |
| `UpdateRunningClock` | Provided a tempo and optionally a new resolution, updates a clock that is currently running. |
| `Destinations` | Returns a read-only list of configured destinations. |
| `Start` | Starts the clock, and optionally sends a MIDI Start message.  |
| `Stop` | Stops the clock, and optionally sends a MIDI Stop message. |
