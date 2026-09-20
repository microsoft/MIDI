---
layout: sdk_reference_page
title: MidiSynthAudioOutputMode
namespace: Windows.Devices.Midi2.Transports.Synth
type: enum
description: How the synthesizer opens the audio device it plays through
---

`MidiSynthAudioOutputMode` chooses how the synthesizer opens the default audio render device.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `WasapiShared` | `0` | Opens the endpoint with the format it already reports, so no device setting is changed and every other application keeps playing |
| `WasapiSharedLowLatency` | `1` | The same, but asks for the smallest period the audio engine will allow |
| `WasapiExclusive` | `2` | Takes the device for the synthesizer alone |

## Remarks

`WasapiShared` is the default and is the right answer for almost every PC.

**`WasapiExclusive` is defined but not implemented yet.** Setting it does not take the device away from everything else on the machine; the synthesizer continues in shared mode. It is in the enumeration because it is coming, and because a settings UI written now should not have to change shape later. Check [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/).`AudioOutputMode` after sending a configuration to see what was actually taken.

**ASIO is deliberately not here.** There is no default ASIO device, so offering it needs a device selection to go with it, and an enumeration value an application can set but the service always refuses is worse than no value at all.

**The synthesizer only holds the audio device while sound is happening.** The device is acquired when a channel voice message arrives and released after a short idle with nothing sounding, so a connected application which is not playing anything is not keeping the device away from anyone. Channel state survives that release, so a song which sets up its instruments and then rests does not come back playing pianos.

If you need the synthesizer to hold no audio device at all, switch it off with `IsEnabled`. See [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/).

## See also

- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
