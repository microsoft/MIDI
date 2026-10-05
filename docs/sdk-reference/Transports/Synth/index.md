---
layout: sdk_namespace_page
title: WinRT API Support for the General MIDI Synthesizer
namespace: Windows.Devices.Midi2.Transports.Synth
description: Namespace for reading and changing the settings of the built-in General MIDI synthesizer
---

Types for reading the state of the built-in General MIDI synthesizer, changing its settings, and finding its endpoint so you can play it.

The synthesizer is a service transport, not a device driver. So it shows up in enumeration as a normal MIDI 2.0 endpoint, along with everything else on the PC. You don't need anything in this namespace to play it. Open a connection to its endpoint, like any other, and send it Universal MIDI Packets. This namespace is for applications that want to *change its settings*, or that want to find its endpoint without enumerating.

Start with the static [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/) class.

## Playing the synthesizer

```csharp
var endpointId = MidiSynthManager.EndpointDeviceId;

if (!string.IsNullOrEmpty(endpointId))
{
    var connection = session.CreateEndpointConnection(endpointId);
    connection.Open();
}
```

`EndpointDeviceId` is empty when the transport isn't installed. It's also empty when the synthesizer is turned off, because then the endpoint doesn't exist. Treat an empty string as "there's no synthesizer to play right now," not as an error.

## Changing a setting

You always send all of the settings at once, not just the one you changed. Build a [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/) from the current [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/), change what you want to change, and send the whole thing back.

```csharp
var config = new MidiSynthConfig(MidiSynthManager.GetStatus());
config.VolumeDecibels = -6.0;

MidiServiceTransportPluginConfigManager.SendUpdate(config);      // apply now
MidiServiceTransportPluginConfigManager.SaveUpdate(config);      // and keep it
```

`SendUpdate` changes the running service. `SaveUpdate` writes the change to the configuration, so it stays after the service restarts. If you send without saving, restarting the service undoes the change. If you save without sending, the change takes effect the next time the service starts.

## Turning the synthesizer off removes its endpoint

Setting `IsEnabled` to false doesn't mute the synthesizer. It removes the endpoint completely and lets go of the audio device. That's on purpose. Some apps open every MIDI port they can find, and so do some Web MIDI pages in a browser. If the endpoint stayed, an app like that would keep the synthesizer open, which keeps the audio device open. Then an app that needs the audio device all to itself, in WASAPI exclusive mode or through ASIO, couldn't have it.

Turning it back on creates the endpoint again with the same device id, so an app that saved the id finds it again.

## Reading the instrument list

A connected app should ask the synthesizer what it can play with MIDI Capability Inquiry Property Exchange, the same way it would ask any other device. The synthesizer answers `ResourceList`, `DeviceInfo`, `ChannelList`, `ChCtrlList`, and a full `ProgramList`. See [How to read a device's patch list]({{ site.baseurl }}/kb/how-to-read-a-device-patch-list/).

[MidiSynthManager.GetMelodicInstruments()]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/) is for when Property Exchange can't help: showing instruments in a settings screen or instrument picker before any connection is open. The list comes from the sound set, so it doesn't change when the settings do.

## Samples

* [C++/WinRT sequence-builder](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/sequence-builder) and [C# sequence-builder](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/sequence-builder) for finding the synthesizer with `EndpointDeviceId` and playing it
* [C++/WinRT midi-file-player](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/midi-file-player) and [C# midi-file-player](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/midi-file-player) for changing the volume while a file plays, without saving it, and putting it back afterward
* [C++/WinRT capability-inquiry-program-list](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/capability-inquiry-program-list) and [C# capability-inquiry-program-list](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/capability-inquiry-program-list) for reading the instrument list with MIDI Capability Inquiry

## See also

- [MIDI Settings]({{ site.baseurl }}/tools/settings/), which has these settings in its global settings dialog
- [MIDI Console]({{ site.baseurl }}/tools/console/), for `midi synth status | enable | disable | configure`
