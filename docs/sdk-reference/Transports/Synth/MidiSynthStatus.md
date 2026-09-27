---
layout: sdk_reference_page
title: MidiSynthStatus
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: What the built-in General MIDI synthesizer is set to right now
---

`MidiSynthStatus` holds the synthesizer's settings at the moment you ask. Get one from [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/).`GetStatus()`.

It's also where you start when you change a setting. [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/) has a constructor that takes one, and that makes it safe to change a single setting.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsEnabled` | Whether the synthesizer is turned on. When false, it has no MIDI endpoint at all and doesn't hold the audio device |
| `RenderMode` | The [MidiSynthRenderMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthRenderModeEnum/) in use |
| `AudioOutputMode` | The [MidiSynthAudioOutputMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthAudioOutputModeEnum/) in use |
| `BankSelectMode` | The [MidiSynthBankSelectMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthBankSelectModeEnum/) in use |
| `VolumeDecibels` | The volume adjustment set by the person using the PC, in decibels. Zero is the standard level |
| `AreEffectsEnabled` | Whether reverb and chorus are turned on |
| `EndpointDeviceId` | The synthesizer's one endpoint. Empty while it's turned off. Pass it to `MidiSession.CreateEndpointConnection` |

## Remarks

**Zero decibels isn't silence. It's the standard level.** The synthesizer is set up to be as loud as the older synthesizer that came with Windows, so existing files sound as loud as they always did. `VolumeDecibels` turns it up or down from there. It's the only volume control the synthesizer has. The service runs in the background, in session 0, and sound from there doesn't get its own slider in the Windows Volume Mixer.

This is separate from the Master Volume that a MIDI file sets with a Universal System Exclusive message. The music controls that one, and a System Reset clears it. This one belongs to the person using the PC, and nothing in a file can change it.

**An empty `EndpointDeviceId` is normal when `IsEnabled` is false.** The endpoint is removed, not muted, so there's no id to give.

**What you read is what's really running.** If a value in the configuration was out of range or the wrong type, it's corrected when it's read. So this reports what the synthesizer is really doing, not what the configuration asked for.

## See also

- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
