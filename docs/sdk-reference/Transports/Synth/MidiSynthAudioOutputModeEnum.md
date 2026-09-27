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
| `WasapiExclusive` | `2` | Takes the device for the synthesizer alone. Not built yet. See the remarks |

## Remarks

`WasapiShared` is the default, and it's the right choice for almost every PC.

**`WasapiExclusive` is listed but not built yet.** Setting it doesn't take the device away from everything else on the PC. The synthesizer keeps using shared mode. It's in the list because it's coming, and so a settings screen written now won't have to change later. After you send a configuration, check [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/).`AudioOutputMode` to see which mode is really in use.

**ASIO is left out on purpose.** There's no default ASIO device, so offering ASIO would also need a way to pick a device. And a value that apps can set but the service always refuses is worse than no value at all.

**The synthesizer only holds the audio device while it's making sound.** It opens the device when a channel voice message arrives, and lets it go after a short quiet time with nothing playing. So a connected app that isn't playing anything isn't keeping the device from anyone else. The channel settings are kept when the device is let go, so a song that sets up its instruments and then pauses doesn't come back playing pianos.

If you need the synthesizer to never hold the audio device, turn it off with `IsEnabled`. See [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/).

## See also

- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
