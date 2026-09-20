---
layout: sdk_reference_page
title: MidiSynthManager
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: The entry point for reading the state of the built-in General MIDI synthesizer and finding its endpoint
---

The entry point for everything to do with the built-in General MIDI synthesizer. All members are static.

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `IsTransportAvailable` | True if the synthesizer transport is installed and loaded in the service. False does not mean the synthesizer is switched off, it means there is no synthesizer on this PC at all |
| `TransportId` | The GUID of this transport |
| `EndpointDeviceId` | The synthesizer's single endpoint. Pass it to `MidiSession.CreateEndpointConnection`. Empty when the transport is not installed, and when the synthesizer is switched off, because then the endpoint does not exist |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `GetStatus()` | A [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/) describing what the synthesizer is set to right now. Null when the transport is not installed or did not answer |
| `GetSoundSetInfo()` | A [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/) describing the active sound set. Null when the transport is not installed or did not answer |
| `GetMelodicInstruments()` | Every melodic instrument in the active sound set as a [MidiSynthInstrumentInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthInstrumentInfo/), with the bank and program which select it. Empty rather than null when nothing answered |
| `SetDrumChannel(channelIndex, isDrumChannel)` | Makes a channel a rhythm part, or stops it being one. Returns false when the channel index is out of range, and when the synthesizer is not currently rendering |

## Remarks

**`EndpointDeviceId` is `GetStatus().EndpointDeviceId`.** It is here so that connecting costs one call rather than two. Both go to the service, so if you need other properties as well, read the status once and use it.

**Everything here is `noexcept` and reports failure by returning null, empty or false.** There is no exception to catch and no error code to interpret. A null status and an empty endpoint identifier both mean "nothing usable right now", whether that is because the transport is missing, the service is not running, or the synthesizer is switched off.

**`SetDrumChannel` is not persisted, on purpose.** The content owns which channels are rhythm parts. A file can ask for this itself with the usual system exclusive message, and a System Reset in a file puts channel 10 back as the only drum channel. This method is the same capability for an application that is not playing a file. Settings which should survive belong in [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/).

`SetDrumChannel` returning false because nothing is rendering is the case worth planning for. The setting lives in the synthesis engine, and the engine only exists while something is connected to the endpoint and sounding. Open your connection first, then set the channel.

The synthesizer occupies a single group, so `SetDrumChannel` takes only a channel index. It is zero-based, so channel 10 as a musician counts it is index 9.

## See also

- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/)
