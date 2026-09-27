---
layout: sdk_reference_page
title: MidiSynthSoundSetInfo
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: What the synthesizer's active sound set contains
---

`MidiSynthSoundSetInfo` describes the sound set the synthesizer is playing. Get one from [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/).`GetSoundSetInfo()`.

It's read-only. The synthesizer plays only the sound set Windows installed. There's no property to point it at a different one, and it never reads a sound file from your app.

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | The name the sound set gives itself |
| `Version` | The sound set's own four-part version number, as text |
| `FilePath` | Where the sound set was loaded from |
| `MelodicInstrumentCount` | How many melodic instruments it has. Drum kits aren't counted here |
| `WaveCount` | How many separate recorded samples it has. It's a measure of the sound set's size, not something you can select |
| `DrumKits` | Every [MidiSynthDrumKitInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthDrumKitInfo/) in the sound set |

## Remarks

**The melodic instruments aren't listed here, on purpose.** There are hundreds of them, and most apps only want the counts. When you do want the list, use [MidiSynthManager.GetMelodicInstruments()]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/). Or better, ask the synthesizer with MIDI Capability Inquiry Property Exchange, which is the standard way to ask any device what it can play. Drum kits are listed because there are only a few, and a bank select can't reach them.

**Reading this doesn't need the synthesizer to be playing**, and it doesn't need a connection. If the sound set isn't loaded yet, it's loaded to answer the question.

## See also

- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
- [MidiSynthDrumKitInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthDrumKitInfo/)
- [MidiSynthInstrumentInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthInstrumentInfo/)
