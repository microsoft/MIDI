---
layout: sdk_reference_page
title: MidiSynthInstrumentInfo
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: One melodic instrument in the synthesizer's sound set, with the bank and program which select it
---

`MidiSynthInstrumentInfo` is one melodic instrument in the active sound set, along with the bank and program numbers that select it. Get them from [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/).`GetMelodicInstruments()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | The instrument's name, as the sound set gives it |
| `BankMsb` | Bank select MSB (controller 0). It starts at zero, the way it's sent in the message |
| `BankLsb` | Bank select LSB (controller 32). It starts at zero, the way it's sent in the message |
| `Program` | The program number. It starts at zero, the way it's sent in the message |

## Remarks

**Use Property Exchange when you have a connection.** Asking the device what it can play with MIDI Capability Inquiry is the standard way to do this. It works for every device, not just this one, and the synthesizer answers with a full `ProgramList`, including names and tags. See [How to read a device's patch list]({{ site.baseurl }}/kb/how-to-read-a-device-patch-list/). This type is for when Property Exchange can't help: choosing an instrument before a connection is open, which is what a settings screen or instrument picker needs.

**These numbers don't change with the settings.** [MidiSynthBankSelectMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthBankSelectModeEnum/) decides how an incoming bank select is *read*, not what's in the sound set. These are the sound set's own numbers, so sending exactly these selects exactly this instrument when the synthesizer is in `RolandGS` mode.

**Program numbers here start at zero.** Instrument charts and front panels usually count from one, so add one before you show a number to a musician.

## See also

- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
- [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/)
- [MidiSynthBankSelectMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthBankSelectModeEnum/)
