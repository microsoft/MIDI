---
layout: sdk_reference_page
title: MidiSynthSoundSetInfo
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: What the synthesizer's active sound set contains
---

`MidiSynthSoundSetInfo` describes the sound set the synthesizer is playing. Get one from [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/).`GetSoundSetInfo()`.

It is read-only. The synthesizer plays the sound set Windows installed and nothing else, so there is no property here to point it somewhere different, and no file a caller supplies is ever parsed.

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | The name the sound set gives itself |
| `Version` | The sound set's own four-part version, as text |
| `FilePath` | Where the sound set was loaded from |
| `MelodicInstrumentCount` | How many melodic instruments it holds. Drum kits are not counted here |
| `WaveCount` | How many individual recorded samples it holds. Useful as a measure of the sound set, not as something to address |
| `DrumKits` | Every [MidiSynthDrumKitInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthDrumKitInfo/) in the sound set |

## Remarks

**The melodic instruments are deliberately not listed here.** There are hundreds of them, and most callers only want the counts. When you do want the list, use [MidiSynthManager.GetMelodicInstruments()]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/), or better, ask the synthesizer over MIDI Capability Inquiry Property Exchange, which is the standard way to ask any device what it can play. Drum kits are here because there are only a handful and they cannot be reached by a bank select.

**Reading this does not need the synthesizer to be sounding**, and does not need a connection. The sound set is loaded to answer the question if nothing has loaded it already.

## See also

- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
- [MidiSynthDrumKitInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthDrumKitInfo/)
- [MidiSynthInstrumentInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthInstrumentInfo/)
