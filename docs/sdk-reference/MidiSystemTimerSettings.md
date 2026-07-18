---
layout: sdk_reference_page
title: MidiSystemTimerSettings
namespace: Windows.Devices.Midi2
type: struct
description: Information about the current Windows system timer configuration
---

This struct is returned by `MidiClock.GetCurrentSystemTimerInfo()` and provides information about the current Windows system timer frequency configuration. This is primarily useful when determining whether to call `MidiClock.BeginLowLatencySystemTimerPeriod()`.

## Struct Fields

| Field | Description |
| ----- | ----------- |
| `CurrentIntervalTicks` | The current timer interrupt interval in 100-nanosecond ticks |
| `MinimumIntervalTicks` | The minimum supported timer interrupt interval in 100-nanosecond ticks |
| `MaximumIntervalTicks` | The maximum supported timer interrupt interval in 100-nanosecond ticks |
