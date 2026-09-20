---
layout: sdk_reference_page
title: MidiSynthStatus
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: What the built-in General MIDI synthesizer is set to right now
---

`MidiSynthStatus` is what the synthesizer is set to at the moment you ask. Get one from [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/).`GetStatus()`.

It is also the starting point for changing a setting: [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/) has a constructor which takes one, which is what makes changing a single property safe.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsEnabled` | Whether the synthesizer is switched on. When false it has no MIDI endpoint at all and holds no audio device |
| `RenderMode` | The [MidiSynthRenderMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthRenderModeEnum/) in use |
| `AudioOutputMode` | The [MidiSynthAudioOutputMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthAudioOutputModeEnum/) in use |
| `BankSelectMode` | The [MidiSynthBankSelectMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthBankSelectModeEnum/) in use |
| `VolumeDecibels` | The customer's own trim, in decibels. Zero is the calibrated level |
| `AreEffectsEnabled` | Whether reverb and chorus are switched on |
| `EndpointDeviceId` | The synthesizer's single endpoint. Empty while it is switched off. Pass it to `MidiSession.CreateEndpointConnection` |

## Remarks

**Zero decibels is not silence, it is the calibrated level.** The synthesizer is calibrated to match the older in-box synthesizer so that existing files sound as loud as they always did. `VolumeDecibels` is a trim either side of that, and it is the only volume control the synthesizer has: the service renders from session 0, so the synthesizer gets no slider of its own in the Windows Volume Mixer.

This is a separate stage from the Master Volume a MIDI file sets with a Universal System Exclusive message. Content owns that one, and a System Reset clears it. This one is the customer's, and nothing in a file can touch it.

**An empty `EndpointDeviceId` is normal when `IsEnabled` is false.** The endpoint is removed rather than muted, so there is nothing to name.

**What you read back is what is running.** A value that was out of range or of the wrong type in the configuration file is corrected when it is read, so this reports what the synthesizer is actually doing rather than what the file asked for.

## See also

- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
