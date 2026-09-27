---
layout: sdk_reference_page
title: MidiSynthConfig
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: The complete set of settings for the built-in General MIDI synthesizer, sent to the service to apply or to save
---

`MidiSynthConfig` holds **all** of the synthesizer's settings. Every property is sent, every time. So build it from the current status, and change only what you want to change.

```csharp
var config = new MidiSynthConfig(MidiSynthManager.GetStatus());
config.VolumeDecibels = -6.0;

MidiServiceTransportPluginConfigManager.SendUpdate(config);
```

This class implements [IMidiServiceTransportPluginConfig]({{ site.baseurl }}/sdk-reference/ServiceConfig/IMidiServiceTransportPluginConfig/). Pass it to `MidiServiceTransportPluginConfigManager.SendUpdate` to change the running service, and to `SaveUpdate` to keep the change after the service restarts.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiSynthConfig()` | Creates a configuration with every property at its default. Sending it puts every setting back to its default |
| `MidiSynthConfig(currentStatus)` | Creates a configuration from what the synthesizer is set to now, using a [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/). This is almost always the one you want |

## Properties

| Property | Default | Description |
| -------- | ------- | ----------- |
| `IsEnabled` | `true` | Whether the synthesizer exists. False removes its endpoint and lets go of the audio device |
| `RenderMode` | `Modern` | The [MidiSynthRenderMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthRenderModeEnum/) |
| `AudioOutputMode` | `WasapiShared` | The [MidiSynthAudioOutputMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthAudioOutputModeEnum/) |
| `BankSelectMode` | `Automatic` | The [MidiSynthBankSelectMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthBankSelectModeEnum/) |
| `VolumeDecibels` | `0.0` | The volume adjustment, in decibels. The service keeps it within the range it supports, which is -60 to +12 right now |
| `AreEffectsEnabled` | `true` | Reverb and chorus. Ignored in `Compatible` render mode, which has neither |

## Remarks

**Start from a status, not from nothing.** A `MidiSynthConfig` made with the empty constructor tells the service to put every setting back to its default, and the service will do it. If you only meant to change the volume, that's a nasty surprise for the person using your app. The constructor that takes a [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/) makes it safe to change just one setting.

**`SendUpdate` and `SaveUpdate` are separate choices.** If you send without saving, restarting the service undoes the change. That's what a preview or "try it" button wants. If you save without sending, the change waits for the next time the service starts. Most settings screens do both.

**Values out of range are adjusted, not refused.** A volume outside the supported range is changed to the nearest limit. Read the status afterward to see what was really used.

**Turning the synthesizer off and on again doesn't change its endpoint device id.** An app that saved the id finds the same endpoint when it comes back.

**Some changes interrupt the sound.** The render mode, sample rate, bank select mode, and effects switch decide how the synthesizer's engine is built. Changing any of them rebuilds the engine, and that clears every channel's program, bank, volume, pan, and tuning. Volume isn't one of them. You can change it at any time without interrupting anything.

## See also

- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
- [MidiServiceTransportPluginConfigManager]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceTransportPluginConfigManager/)
