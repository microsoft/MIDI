---
layout: sdk_reference_page
title: MidiSynthBankSelectMode
namespace: Windows.Devices.Midi2.Transports.Synth
type: enum
description: How the synthesizer reads an incoming bank select, so files written for different conventions find the right instrument
---

`MidiSynthBankSelectMode` decides how an incoming bank select is read.

The built-in sound set is addressed the Roland GS way: the variation lives in the bank MSB, controller 0, and the LSB is always zero. A file written for Yamaha XG puts the variation in the LSB instead, and a file written for General MIDI 2 fixes the MSB at 121. On either of those, every variation lookup misses and the sound falls back to the plain instrument. This setting is how that is fixed.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `RolandGS` | `0` | Native addressing for the built-in sound set |
| `YamahaXG` | `1` | Variation in the LSB. MSB 127 and 126 select drum kits |
| `GeneralMidi2` | `2` | MSB 121 melodic, MSB 120 drum kit, variation in the LSB |
| `Automatic` | `3` | Follow whichever System On or reset the sender last sent |

## Remarks

`Automatic` is the default. It starts from `RolandGS` and moves only when a sender identifies itself with a System On or a reset. **An explicit choice is never overridden by what a file claims**, so if you set one of the other three, it stays set.

**This is worth having because of how real files are written, not because of what the specifications say.** Across a survey of 175,163 real MIDI files, XG-style bank selects outnumbered General MIDI 2 style ones by more than twenty to one, and neither is addressed the way the built-in sound set stores its variations. A General MIDI 2 System On appeared ten times in the whole corpus; a General MIDI 1 System On appeared over thirty-five thousand times.

**This does not change what the sound set contains**, only how an address is interpreted. [MidiSynthInstrumentInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthInstrumentInfo/) always reports the sound set's own addressing.

**A mode which does not match the file is not an error, it is a wrong instrument.** A variation lookup that misses falls back to the capital tone, so the file plays, it just plays on the plain version of the sound. If a customer reports that a file sounds close but not right, this is the setting to check.

Changing bank select mode rebuilds the synthesis engine, which clears every channel's program, bank, volume, pan and tuning. Do not change it while music is playing.

## See also

- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthInstrumentInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthInstrumentInfo/)
