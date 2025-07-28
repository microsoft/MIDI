---
layout: sdk_reference_page
title: MidiClockDestination
namespace: Microsoft.Windows.Devices.Midi2.Utilities.Sequencing
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Supports the simple MIDI Beat Clock generator
status: preview
---

This represents a destination for the MIDI Beat Clock. It is provided with an opened connection and a list of MIDI 2.0 Groups (these also correspond to MIDI 1.0 ports) to send the messages to.

## Functions

| Function | Description |
| --------------- | ----------- |
| `MidiClockDestination` | Construct a destination with an opened valid connection and a list of MIDI 2.0 groups |
| `Connection` | Provides access to the connection used to construct this type |
| `Groups` | Returns a read-only list of configured groups. |
