---
layout: sdk_reference_page
title: MidiSequenceBarPosition
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: struct
description: Where a tick falls in bars and beats
---

`MidiSequenceBarPosition` is where a tick falls in the music, in the terms a player and a score use. Returned by `GetBarPositionAtTick` on [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/).

## Struct Fields

| Field | Description |
| ----- | ----------- |
| `Bar` | The bar number |
| `Beat` | The beat within the bar |
| `TicksIntoBeat` | How far into that beat the tick falls |

## Remarks

**Bars and beats are counted from one**, the way a player and a score show them, not from zero. Display them as they are.
