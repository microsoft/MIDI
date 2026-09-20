---
layout: sdk_reference_page
title: MidiSynthConfig
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: The complete set of settings for the built-in General MIDI synthesizer, sent to the service to apply or to save
---

`MidiSynthConfig` carries the synthesizer's settings as a **complete set**. Every property here is written when the object is sent, so build it from the current status and change only what you mean to change.

```csharp
var config = new MidiSynthConfig(MidiSynthManager.GetStatus());
config.VolumeDecibels = -6.0;

MidiServiceTransportPluginConfigManager.SendUpdate(config);
```

This class implements [IMidiServiceTransportPluginConfig]({{ site.baseurl }}/sdk-reference/ServiceConfig/IMidiServiceTransportPluginConfig), so it is passed to `MidiServiceTransportPluginConfigManager.SendUpdate` to apply it to the running service, and to `SaveUpdate` to keep it across a service restart.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiSynthConfig()` | An empty configuration. Every property is at its default, so sending this returns every setting to its default |
| `MidiSynthConfig(currentStatus)` | Starts from what the synthesizer is set to now, taken from a [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/). This is the one you almost always want |

## Properties

| Property | Default | Description |
| -------- | ------- | ----------- |
| `IsEnabled` | `true` | Whether the synthesizer exists. False removes its endpoint and releases the audio device |
| `RenderMode` | `Modern` | The [MidiSynthRenderMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthRenderModeEnum/) |
| `AudioOutputMode` | `WasapiShared` | The [MidiSynthAudioOutputMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthAudioOutputModeEnum/) |
| `BankSelectMode` | `Automatic` | The [MidiSynthBankSelectMode]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthBankSelectModeEnum/) |
| `VolumeDecibels` | `0.0` | The customer's trim. Clamped by the service to the range it supports, currently -60 to +12 |
| `AreEffectsEnabled` | `true` | Reverb and chorus. Ignored in `Compatible` render mode, which has neither |

## Remarks

**Construct from a status, not from nothing.** A default-constructed `MidiSynthConfig` is a valid instruction to return every setting to its default, and the service will carry it out. If you only meant to change the volume, that is a surprise for the customer. The constructor which takes a [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/) exists so a single-property change is safe.

**`SendUpdate` and `SaveUpdate` are separate decisions.** Sending without saving offers a change the customer can undo by restarting the service, which is what a preview or an audition button wants. Saving without sending stages a change for the next service start. Most settings UI does both.

**Values are clamped, not refused.** A volume outside the supported range becomes the nearest bound. Read the status back afterwards to see what was actually taken.

**Turning the synthesizer off and on again does not change its endpoint identifier.** An application which remembered the identifier finds the same endpoint when it comes back.

**A render mode change interrupts sound.** Render mode, sample rate, bank select mode and the effects switch decide how the synthesis engine is built, so changing any of them rebuilds it, and that clears every channel's program, bank, volume, pan and tuning. Volume is not in that set and can be changed at any time without disturbing anything.

## See also

- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
- [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/)
- [MidiServiceTransportPluginConfigManager]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceTransportPluginConfigManager)
