---
layout: sdk_reference_page
title: MidiSynthRenderMode
namespace: Windows.Devices.Midi2.Transports.Synth
type: enum
description: Whether the synthesizer reproduces the older in-box synthesizer or plays the same sound set without its historic limits
---

`MidiSynthRenderMode` chooses how the synthesizer makes its sound. Both modes use the same sound set. The difference is whether the synthesizer also copies the limits of the older synthesizer that came with Windows.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Compatible` | `0` | Sounds like the older synthesizer that came with Windows, limits and all: 32 voices, linear interpolation, no reverb or chorus, and a 22050 Hz sample rate. That lower sample rate cuts off high frequencies, and that's part of the sound being copied |
| `Modern` | `1` | The same sound set, without the old limits |

## Remarks

`Modern` is the default. It's what you want unless someone is comparing the sound against an older PC.

**`Compatible` isn't a lower-quality setting. It's an accuracy setting.** The 22050 Hz sample rate, the voice count, and the missing effects are what that synthesizer sounded like. A file made for it was mixed with that sound in mind. Sounding the same means copying the limits too.

**Effects are off in `Compatible` mode, whatever `AreEffectsEnabled` says**, because the older synthesizer didn't have them.

Changing the render mode rebuilds the synthesizer's engine. That clears every channel's program, bank, volume, pan, and tuning, so don't change it while music is playing.

## See also

- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
