---
layout: sdk_reference_page
title: MidiSystemTimerSettings
namespace: Windows.Devices.Midi2
type: struct
description: Information about the current Windows system timer configuration
---

`MidiClock.GetCurrentSystemTimerInfo()` returns this struct. It tells you how often the Windows system timer fires right now, and the fastest and slowest it can go. Use it to decide whether calling `MidiClock.BeginLowLatencySystemTimerPeriod()` would help.

## Struct Fields

| Field | Description |
| ----- | ----------- |
| `CurrentIntervalTicks` | The time between timer interrupts right now, in 100-nanosecond units |
| `MinimumIntervalTicks` | The shortest time between timer interrupts this PC supports, in 100-nanosecond units |
| `MaximumIntervalTicks` | The longest time between timer interrupts this PC supports, in 100-nanosecond units |
