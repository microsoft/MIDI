---
layout: sdk_namespace_page
title: WinRT API Endpoint Enumeration Overview
namespace: Windows.Devices.Midi2.Enumeration
description: Namespace for finding the MIDI endpoints on the PC and reading what they can do
---

This is where an application finds out what MIDI devices are on the PC, and what each of them can do. It's the first namespace most applications touch.

[`MidiEndpointDeviceWatcher`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceWatcher/) is the type to reach for. It reports the endpoints present when it starts, and then keeps reporting as devices arrive, change, and go away. Enumerating once and caching the result is not enough on a modern PC: USB devices are unplugged, Bluetooth devices come and go on their own, and network devices appear when the far end is switched on.

Each endpoint is a [`MidiEndpointDeviceInformation`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceInformation/), which is both the thing you display in a device list and the source of the endpoint device id you pass to `MidiSession.CreateEndpointConnection`.

## What an endpoint tells you about itself, and who said so

The properties of an endpoint come from several different places, and the namespace keeps them apart on purpose, because they do not carry the same authority.

| Source | Type | What it is |
| ------ | ---- | ---------- |
| The device, in protocol | [`MidiDeclaredEndpointInfo`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiDeclaredEndpointInfo/), [`MidiDeclaredDeviceIdentity`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiDeclaredDeviceIdentity/), [`MidiDeclaredStreamConfiguration`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiDeclaredStreamConfiguration/) | What the device said about itself when it was asked, using UMP Stream messages |
| The transport | [`MidiEndpointTransportSuppliedInfo`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointTransportSuppliedInfo/) | What the transport knows, such as the name Windows has for the device and how it is connected |
| The customer | [`MidiEndpointUserSuppliedInfo`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointUserSuppliedInfo/) | A name, description, image, and latency compensation someone set in MIDI Settings |

A name a customer typed outranks a name a device declared, which outranks a name a transport worked out. Use the `Name` property on the endpoint rather than picking one of these yourself, and it resolves that for you.

## Groups and channels

[`MidiFunctionBlock`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiFunctionBlock/) is how a MIDI 2.0 device says which of its sixteen groups do what, which way they flow, and what to call them. [`MidiGroupTerminalBlock`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiGroupTerminalBlock/) is the USB descriptor equivalent, available for a USB device which declares no function blocks.

**Prefer function blocks when both are present.** A function block is what the device is saying now, and it can change while the device is connected; a group terminal block is static USB descriptor data read once at enumeration.

## Timing, which is the part that catches people out

Endpoint discovery is a conversation with the device, so an endpoint's `Added` event can arrive before the device has finished answering. `IsEndpointDiscoveryComplete` tells you whether that conversation is done. Treat it as a hint about when function blocks and declared names are worth reading, **not** as a gate on opening a connection &mdash; a transport which does no in-protocol discovery sets it as soon as the endpoint exists.

[`MidiEndpointDeviceInformationUpdatedEventArgs`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceInformationUpdatedEventArgs/) carries a set of flags saying which group of properties changed, so an update handler can re-read only what moved rather than everything.

The MIDI 1.0 ports for an endpoint are separate Windows device nodes, created after the endpoint itself, and they raise their own notifications with no ordering relationship to the endpoint's. An application which needs the ports has to watch for the ports, using `MidiLegacyPortDeviceWatcher` in [`Windows.Devices.Midi2.Enumeration.Legacy`]({{ site.baseurl }}/sdk-reference/Enumeration/Legacy/).

See [Endpoint arrival and update ordering]({{ site.baseurl }}/kb/endpoint-arrival-and-update-ordering/) for what is and is not guaranteed here, and how to write a watcher that's correct anyway.

## Samples

* [C++/WinRT watch-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-endpoints)
* [C# watch-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/watch-endpoints)
