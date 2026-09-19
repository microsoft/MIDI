---
layout: sdk_reference_page
title: MidiSequencePlayerPosition
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: struct
description: Everything a transport display needs, read in one call
---

`MidiSequencePlayerPosition` is everything a transport display needs, read in one call. It is the `Position` property of [`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/).
## Struct Fields

| Field | Description |
| ----- | ----------- |
| `State` | The [`MidiSequencePlayerState`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayerStateEnum/) at the moment of the read |
| `Microseconds` | Current position in time |
| `DurationMicroseconds` | Total length of the loaded sequence |
| `Tick` | Current position in ticks |
| `Bar` | Current bar, counted from one |
| `Beat` | Current beat within the bar, counted from one |
| `BeatsPerMinute` | The tempo at this position, not for the whole sequence |

## Remarks

**Poll this rather than asking for an event.** This is a struct rather than an event because a display refreshes tens of times a second, and an event per frame would cost more than the drawing does. There is deliberately no position event on the player.

Every field is a snapshot taken together, so bar, beat and time cannot disagree with one another the way they could if you read four separate properties.
