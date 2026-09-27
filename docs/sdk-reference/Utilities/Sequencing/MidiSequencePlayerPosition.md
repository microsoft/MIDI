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
| `State` | The [`MidiSequencePlayerState`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayerStateEnum/) when it was read |
| `Microseconds` | The position in time right now |
| `DurationMicroseconds` | The total length of the loaded sequence |
| `Tick` | The position in ticks right now |
| `Bar` | The bar right now, counted from one |
| `Beat` | The beat within the bar right now, counted from one |
| `BeatsPerMinute` | The tempo at this position, not for the whole sequence |

## Remarks

**Read this on a timer, instead of waiting for an event.** It's a struct, not an event, because a display updates tens of times a second, and an event for every frame would cost more than the drawing does. The player has no position event, on purpose.

Every field is read at the same moment, so the bar, beat, and time can't disagree with each other, the way they could if you read four separate properties.
