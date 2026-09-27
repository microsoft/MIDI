---
layout: sdk_reference_page
title: MidiSynthBankSelectMode
namespace: Windows.Devices.Midi2.Transports.Synth
type: enum
description: How the synthesizer reads an incoming bank select, so files written for different conventions find the right instrument
---

`MidiSynthBankSelectMode` decides how an incoming bank select is read.

The built-in sound set is laid out the Roland GS way. The variation is in the bank MSB (controller 0), and the LSB is always zero. A file written for Yamaha XG puts the variation in the LSB instead, and a file written for General MIDI 2 always sets the MSB to 121. With either of those, the synthesizer can't find the variation, so it plays the plain instrument instead. This setting fixes that.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `RolandGS` | `0` | The way the built-in sound set is laid out |
| `YamahaXG` | `1` | The variation is in the LSB. MSB 127 and 126 select drum kits |
| `GeneralMidi2` | `2` | MSB 121 is melodic, MSB 120 is a drum kit, and the variation is in the LSB |
| `Automatic` | `3` | Follows whichever System On or reset message the sender sent last |

## Remarks

`Automatic` is the default. It starts with `RolandGS`, and changes only when a sender says which kind it is with a System On or reset message. **A mode you choose is never changed by what a file says**, so if you set one of the other three, it stays set.

**This matters because of how real files are written, not because of what the specifications say.** In a survey of 175,163 real MIDI files, XG-style bank selects outnumbered General MIDI 2-style ones by more than twenty to one. Neither matches the way the built-in sound set stores its variations. A General MIDI 2 System On message showed up only ten times in all those files. A General MIDI 1 System On showed up more than thirty-five thousand times.

**This doesn't change what's in the sound set**, only how a bank select is read. [MidiSynthInstrumentInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthInstrumentInfo/) always reports the sound set's own bank and program numbers.

**A mode that doesn't match the file isn't an error. It's a wrong instrument.** When the synthesizer can't find a variation, it uses the main version of that instrument instead. So the file still plays, just with the plain version of the sound. If someone says a file sounds close but not quite right, check this setting.

Changing the bank select mode rebuilds the synthesizer's engine. That clears every channel's program, bank, volume, pan, and tuning, so don't change it while music is playing.

## See also

- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthInstrumentInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthInstrumentInfo/)
