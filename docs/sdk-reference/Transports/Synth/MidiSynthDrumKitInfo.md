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
| `Program` | The program number that selects it. It starts at zero, the way it's sent in the message |

## Remarks

**You select a kit with a program change on a drum channel, not with a bank select.** Send the program change on channel 10, or on any other channel you've made a drum channel with [MidiSynthManager.SetDrumChannel]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/). The same program number on a melodic channel selects a melodic instrument instead.

`Program` starts at zero, so add one before you show it to a musician.

## See also

- [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/)
- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
