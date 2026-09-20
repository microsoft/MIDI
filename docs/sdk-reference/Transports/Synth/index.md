---
layout: sdk_namespace_page
title: WinRT API Support for the General MIDI Synthesizer
namespace: Windows.Devices.Midi2.Transports.Synth
description: Namespace for reading and changing the settings of the built-in General MIDI synthesizer
---

Types for reading the state of the built-in General MIDI synthesizer, changing its settings, and finding its endpoint so you can play it.

The synthesizer is a service transport, not a device driver, so it appears in enumeration as an ordinary MIDI 2.0 endpoint alongside everything else on the PC. You do not need anything in this namespace to play it. Open a connection to its endpoint like any other and send it Universal MIDI Packets. This namespace is for applications that want to *configure* it, or that want to find its endpoint without enumerating.

Everything is reached through the static [MidiSynthManager]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/) class.

## Playing the synthesizer

```csharp
var endpointId = MidiSynthManager.EndpointDeviceId;

if (!string.IsNullOrEmpty(endpointId))
{
    var connection = session.CreateEndpointConnection(endpointId);
    connection.Open();
}
```

`EndpointDeviceId` is empty when the transport is not installed, and also when the synthesizer is switched off, because then the endpoint genuinely does not exist. Treat an empty string as "there is no synthesizer to play right now" rather than as an error.

## Changing a setting

Settings are a complete set, not a patch. Build a [MidiSynthConfig]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthConfig/) from the current [MidiSynthStatus]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthStatus/), change what you mean to change, and send the whole thing back.

```csharp
var config = new MidiSynthConfig(MidiSynthManager.GetStatus());
config.VolumeDecibels = -6.0;

MidiServiceTransportPluginConfigManager.SendUpdate(config);      // apply now
MidiServiceTransportPluginConfigManager.SaveUpdate(config);      // and keep it
```

`SendUpdate` applies the change to the running service. `SaveUpdate` writes it to the configuration so it survives a service restart. Sending without saving gives the customer a change they can undo by restarting; saving without sending stages one for next time.

## Switching the synthesizer off removes its endpoint

`IsEnabled` set to false does not mute the synthesizer, it removes the endpoint entirely and releases the audio device. That is deliberate. An application that opens every MIDI port it can find — which some browser-based Web MIDI pages do — would otherwise hold the synthesizer endpoint open, which holds the audio device, which keeps it away from an application that wants it in WASAPI exclusive mode or through ASIO.

Enabling it again recreates the endpoint with the same device identifier, so an application that remembered the identifier finds it again.

## Reading the instrument list

A connected application should ask the synthesizer what it can play using MIDI Capability Inquiry Property Exchange, the same way it would ask any other device. The synthesizer answers `ResourceList`, `DeviceInfo`, `ChannelList` and a full `ProgramList`. See [How to read a device's patch list]({{ site.baseurl }}/kb/how-to-read-a-device-patch-list/).

[MidiSynthManager.GetMelodicInstruments()]({{ site.baseurl }}/sdk-reference/Transports/Synth/MidiSynthManager/) exists for the case Property Exchange cannot serve: browsing instruments in a settings or patch-picking UI before any connection is open. The list is a property of the sound set, so it does not change with configuration.

## See also

- [MIDI Settings]({{ site.baseurl }}/tools/settings/), which has these settings in its global settings dialog
- [MIDI Console]({{ site.baseurl }}/tools/console/), for `midi synth status | enable | disable | configure`
