---
layout: sdk_reference_page
title: MidiSynthRenderMode
namespace: Windows.Devices.Midi2.Transports.Synth
type: enum
description: Whether the synthesizer reproduces the older in-box synthesizer or plays the same sound set without its historic limits
---

`MidiSynthRenderMode` chooses how the synthesizer renders. Both modes use the same sound set; what differs is whether the limits of the older in-box synthesizer are reproduced along with it.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Compatible` | `0` | Reproduces the observable behavior of the older in-box synthesizer, limits included: 32 voices, linear interpolation, no reverb or chorus, and a 22050 Hz render rate whose bandwidth limit is part of the sound being reproduced |
| `Modern` | `1` | The same sound set without the historic limits |

## Remarks

`Modern` is the default, and is what you want unless a customer is comparing output against an older PC.

**`Compatible` is not a lower-quality setting, it is a fidelity setting.** The 22050 Hz render rate, the voice count and the missing effects are what that synthesizer sounded like, and a file authored against it was mixed with that sound in mind. Reproducing it means reproducing the limits.

**Effects are off in `Compatible` mode regardless of `AreEffectsEnabled`**, because the synthesizer being reproduced did not have them.

Changing render mode rebuilds the synthesis engine, which clears every channel's program, bank, volume, pan and tuning. Do not change it while music is playing.

## See also

- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
