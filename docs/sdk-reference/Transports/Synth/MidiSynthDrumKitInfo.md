---
layout: sdk_reference_page
title: MidiSynthDrumKitInfo
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: One drum kit in the synthesizer's sound set
---

`MidiSynthDrumKitInfo` is one drum kit in the active sound set. Get them from the `DrumKits` collection on [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/).

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | The kit's name, as the sound set gives it |
| `Program` | The program number which selects it. Zero-based, as it appears on the wire |

## Remarks

**A kit is selected by a program change on a drum channel, not by a bank select.** Send the program change on channel 10, or on whichever other channel you have made a rhythm part with [MidiSynthManager.SetDrumChannel]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/). Sending the same program number on a melodic channel selects a melodic instrument instead.

`Program` is zero-based, so add one before showing it to a musician.

## See also

- [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/)
- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
