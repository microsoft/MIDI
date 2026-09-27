---
layout: sdk_namespace_page
title: WinRT API Endpoint Enumeration Overview
namespace: Windows.Devices.Midi2.Enumeration
description: Namespace for finding the MIDI endpoints on the PC and reading what they can do
---

This is where an application finds out what MIDI devices are on the PC, and what each of them can do. It's the first namespace most applications touch.

[`MidiEndpointDeviceWatcher`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceWatcher/) is the type to use. It reports the endpoints that are there when it starts, and then keeps reporting as devices arrive, change, and go away. Listing the endpoints once and saving the result isn't enough on a modern PC: USB devices get unplugged, Bluetooth devices come and go on their own, and network devices show up when the other end is switched on.

Each endpoint is a [`MidiEndpointDeviceInformation`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceInformation/), which is both the thing you display in a device list and the source of the endpoint device id you pass to `MidiSession.CreateEndpointConnection`.

## What an endpoint tells you about itself, and who said so

The properties of an endpoint come from several different places. The namespace keeps them apart on purpose, because they don't all carry the same weight.

| Source | Type | What it is |
| ------ | ---- | ---------- |
| The device, in protocol | [`MidiDeclaredEndpointInfo`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiDeclaredEndpointInfo/), [`MidiDeclaredDeviceIdentity`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiDeclaredDeviceIdentity/), [`MidiDeclaredStreamConfiguration`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiDeclaredStreamConfiguration/) | What the device said about itself when it was asked, using UMP Stream messages |
| The transport | [`MidiEndpointTransportSuppliedInfo`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointTransportSuppliedInfo/) | What the transport knows, such as the name Windows has for the device and how it is connected |
| The customer | [`MidiEndpointUserSuppliedInfo`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointUserSuppliedInfo/) | A name, description, image, and latency compensation someone set in MIDI Settings |

A name the customer typed wins over a name the device gave, and a name the device gave wins over a name the transport came up with. Use the endpoint's `Name` property instead of choosing one of these yourself. It makes that choice for you.

## Groups and channels

[`MidiFunctionBlock`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiFunctionBlock/) is how a MIDI 2.0 device says which of its sixteen groups do what, which way they send, and what to call them. [`MidiGroupTerminalBlock`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiGroupTerminalBlock/) is the same idea from the USB descriptors, for a USB device that has no function blocks.

**Use function blocks when a device has both.** A function block is what the device is saying now, and it can change while the device is connected. A group terminal block comes from the USB descriptors, which are read once when the device is connected and don't change.

## Timing

Endpoint discovery is a back-and-forth with the device, so an endpoint's `Added` event can arrive before the device has finished answering. `IsEndpointDiscoveryComplete` tells you whether that's done. Use it as a hint about when function blocks and declared names are worth reading, **not** as something to wait for before you open a connection. A transport that doesn't do discovery in the protocol sets it as soon as the endpoint exists.

[`MidiEndpointDeviceInformationUpdatedEventArgs`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceInformationUpdatedEventArgs/) has a set of flags that say which groups of properties changed, so your update handler can read again only what changed, instead of everything.

An endpoint's MIDI 1.0 ports are separate Windows devices, created after the endpoint itself. They raise their own notifications, and those can come in any order compared with the endpoint's. An application that needs the ports has to watch for the ports, using `MidiLegacyPortDeviceWatcher` in [`Windows.Devices.Midi2.Enumeration.Legacy`]({{ site.baseurl }}/sdk-reference/Enumeration/Legacy/).

To learn what is and isn't guaranteed here, and how to write a watcher that works anyway, see [Endpoint arrival and update ordering]({{ site.baseurl }}/kb/endpoint-arrival-and-update-ordering/).

## Samples

* [C++/WinRT watch-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-endpoints)
* [C# watch-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/watch-endpoints)
