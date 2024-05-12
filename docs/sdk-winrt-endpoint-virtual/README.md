---
layout: api_group_page
title: Virtual Devices
parent: Microsoft.Devices.Midi2.Endpoints.Virtual
has_children: true
---

# Virtual Devices

Fully-featured app-to-app MIDI in a MIDI 2.0 world involves connections to a virtual device which must participate in the full MIDI 2.0 protocol, from discovery through protocol negotiation. To support this scenario, the way app-to-app MIDI works in Windows MIDI Services is for an application to define a device and then using the `MidiSession`, construct that device's endpoint. Once the device endpoint is opened, Windows MIDI Services will then construct a second application-visible multi-client endpoint which applications will use to talk to the device app. 

During that conversation, the service will also handle discovery and protocol negotiation with the virtual device just like it would any physical device. 

In addition to the service component, it is implemented in the client API as a type of Client-Side Processing Plugin

## Sample

[Full C# Sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/app-to-app-midi-cs)

