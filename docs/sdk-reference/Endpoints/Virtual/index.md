---
layout: sdk_namespace_page
title: Virtual
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Virtual
library: Microsoft.Windows.Devices.Midi2
description: Namespace for virtual / app-to-app MIDI management
---

Fully-featured app-to-app MIDI in a MIDI 2.0 world involves connections to a virtual device which must participate in the full MIDI 2.0 protocol, from discovery through protocol negotiation. To support this scenario, the way app-to-app MIDI works in Windows MIDI Services is for an application to define a device and then using the `MidiVirtualDeviceManager`, construct that device's endpoint. Once the device endpoint is opened (like any other connection), Windows MIDI Services will then construct a second application-visible multi-client endpoint which applications will use to talk to the device app. 

During that conversation, the service will also handle discovery and protocol negotiation with the virtual device just like it would any physical device. 

In addition to the service component, it is implemented in the client API as a type of Client-Side Processing Plugin `MidiVirtualDevice`

## Examples

[C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/simple-app-to-app-midi/main.cpp)
[C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/virtual-device-app-winui)
