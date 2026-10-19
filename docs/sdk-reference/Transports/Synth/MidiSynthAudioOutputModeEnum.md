---
layout: sdk_reference_page
title: MidiSynthAudioOutputMode
namespace: Windows.Devices.Midi2.Transports.Synth
type: enum
description: How the synthesizer opens the audio device it plays through
---

`MidiSynthAudioOutputMode` chooses how the synthesizer opens the default audio output device.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `WasapiShared` | `0` | Opens the audio device with the format it already uses, so no device setting changes and every other app keeps playing |
| `WasapiSharedLowLatency` | `1` | The same, but asks the Windows audio engine for the shortest processing period it allows, which lowers latency |
| `WasapiExclusive` | `2` | Takes the device for the synthesizer alone while it plays, which gives the shortest delay. Nothing else can play through it until the synthesizer lets it go. See the remarks |

## Remarks

`WasapiShared` is the default, and it's the right choice for almost every PC.

**`WasapiExclusive` skips the Windows audio mixer.** The synthesizer opens the device directly, at the sample rate the device already uses and with the shortest buffer the device allows, so a note sounds sooner after it arrives. While the synthesizer holds the device, no other app can play sound through it.

**Exclusive mode falls back to shared mode when the device can't be taken.** That happens when another app is using the device exclusively, when Windows is set not to allow exclusive use of the device, or when the device accepts no sample format the synthesizer can write. The synthesizer plays in shared mode for that time, so it's still heard, and tries exclusive mode again the next time it opens the device. [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/).`AudioOutputMode` reports the setting, so it still reads `WasapiExclusive` then.

**ASIO is left out on purpose.** There's no default ASIO device, so offering ASIO would also need a way to pick a device. And a value that apps can set but the service always refuses is worse than no value at all.

**The synthesizer only holds the audio device while it's making sound.** It opens the device when a channel voice message arrives, and lets it go after a short quiet time with nothing playing. So a connected app that isn't playing anything isn't keeping the device from anyone else. The channel settings are kept when the device is let go, so a song that sets up its instruments and then pauses doesn't come back playing pianos.

If you need the synthesizer to never hold the audio device, turn it off with `IsEnabled`. See [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/).

## See also

- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
