---
layout: sdk_reference_page
title: MidiSynthManager
namespace: Windows.Devices.Midi2.Transports.Synth
type: runtimeclass
description: The entry point for reading the state of the built-in General MIDI synthesizer and finding its endpoint
---

Start here for anything to do with the built-in General MIDI synthesizer. All members are static.

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `IsTransportAvailable` | True if the synthesizer transport is installed and loaded in the service. False doesn't mean the synthesizer is turned off. It means there's no synthesizer on this PC at all |
| `TransportId` | The GUID of this transport |
| `EndpointDeviceId` | The synthesizer's one endpoint. Pass it to `MidiSession.CreateEndpointConnection`. Empty when the transport isn't installed, or when the synthesizer is turned off, because then the endpoint doesn't exist |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `GetStatus()` | Returns a [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/) with the synthesizer's settings right now. Null when the transport isn't installed or didn't answer |
| `GetSoundSetInfo()` | Returns a [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/) that describes the active sound set. Null when the transport isn't installed or didn't answer |
| `GetMelodicInstruments()` | Returns every melodic instrument in the active sound set as a [MidiSynthInstrumentInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthInstrumentInfo/), with the bank and program that select it. Empty, not null, when nothing answered |
| `SetDrumChannel(channelIndex, isDrumChannel)` | Makes a channel a drum channel, or turns that off. Returns false if the channel index is out of range, or if the synthesizer isn't making sound right now |

## Remarks

**`EndpointDeviceId` is the same as `GetStatus().EndpointDeviceId`.** It's here so connecting takes one call instead of two. Both ask the service, so if you need other properties too, read the status once and use that.

**Nothing here throws an exception.** Instead, a failure returns null, empty, or false, so there's no exception to catch and no error code to look up. A null status and an empty endpoint device id both mean "nothing to use right now." That might be because the transport is missing, the service isn't running, or the synthesizer is turned off.

**`SetDrumChannel` isn't saved, on purpose.** The music decides which channels are drum channels. A file can set this itself with the usual System Exclusive message, and a System Reset in a file makes channel 10 the only drum channel again. This method lets an app that isn't playing a file do the same thing. Settings that should be kept belong in [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/).

Plan for `SetDrumChannel` returning false because the synthesizer isn't making sound. The setting lives in the synthesizer's engine, and the engine only exists while something is connected to the endpoint and playing. Open your connection first, and then set the channel.

The synthesizer uses only one group, so `SetDrumChannel` takes just a channel index. The index starts at zero, so channel 10, as a musician counts it, is index 9.

## See also

- [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/)
- [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/)
- [MidiSynthSoundSetInfo]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthSoundSetInfo/)
