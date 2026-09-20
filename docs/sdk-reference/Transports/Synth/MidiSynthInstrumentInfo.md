---
layout: sdk_reference_page
title: MidiSynthInstrumentInfo
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: One melodic instrument in the synthesizer's sound set, with the bank and program which select it
---

`MidiSynthInstrumentInfo` is one melodic instrument in the active sound set, together with the address that selects it. Get them from [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/).`GetMelodicInstruments()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | The instrument's name, as the sound set gives it |
| `BankMsb` | Bank select MSB, controller 0. Zero-based, as it appears on the wire |
| `BankLsb` | Bank select LSB, controller 32. Zero-based, as it appears on the wire |
| `Program` | Program number. Zero-based, as it appears on the wire |

## Remarks

**Prefer Property Exchange when you have a connection.** Asking the device what it can play over MIDI Capability Inquiry is the standard way to do this, it works for every device rather than only this one, and the synthesizer answers a full `ProgramList` with names and tags. See [How to read a device's patch list]({{ site.baseurl }}/kb/how-to-read-a-device-patch-list/). This type is for the case Property Exchange cannot serve: choosing an instrument before a connection is open, which is what a settings or patch-picking UI wants to do.

**These addresses do not change with configuration.** [MidiSynthBankSelectMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthBankSelectModeEnum/) decides how an incoming bank select is *read*, not what the sound set contains. The values here are the sound set's own addressing, so sending exactly these will select exactly this instrument when the synthesizer is in `RolandGS` mode.

**Program numbers here are zero-based.** Instrument charts and front panels usually count from one, so add one before showing a number to a musician.

## See also

- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
- [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/)
- [MidiSynthBankSelectMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthBankSelectModeEnum/)
