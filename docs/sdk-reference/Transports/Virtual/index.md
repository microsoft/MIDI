---
layout: sdk_namespace_page
title: WinRT API Support for app-to-app Virtual Devices
namespace: Windows.Devices.Midi2.Transports.Virtual
description: Namespace for virtual / app-to-app MIDI management
---

In MIDI 2.0, full app-to-app MIDI means connecting to a virtual device that takes part in the whole MIDI 2.0 protocol, from discovery to protocol negotiation. So in Windows MIDI Services, one app defines a device, and then uses `MidiVirtualDeviceManager` to create that device's endpoint. When the app opens the device endpoint, like any other connection, Windows MIDI Services creates a second endpoint. Other apps see that second endpoint, and many of them can use it at once to talk to the device app.

The service also handles discovery and protocol negotiation with the virtual device, just as it would with a physical device.

In the client API, the virtual device is a message processing plugin: `MidiVirtualDevice`.

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/simple-app-to-app-midi)
* [C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
