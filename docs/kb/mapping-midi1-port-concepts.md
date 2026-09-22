---
layout: kb
title: Understanding How we Map MIDI 1.0 Ports to UMP Endpoints
audience: everyone
description: This document explains how MIDI 1.0 ports relate to MIDI 2.0 UMP endpoints
categories:
  - Internals
---

The big change between MIDI 1.0 and MIDI 2.0 UMP (Universal MIDI Packet) endpoints is that there are no longer any "ports".

## Background

In USB MIDI 1.0, messages travel over USB in a 32-bit packet. That packet carries a virtual cable number, which picks one of the 16 possible virtual cables on the endpoint. That's how a single MIDI 1.0 device can have, say, 8 input ports and 8 output ports on one connection.

MIDI 2.0 UMP does something similar, but the equivalent of a cable is called a group. The group number is part of the message itself instead of being sent alongside it, so everything needed to route a message inside an endpoint travels with the message.

Some UMP messages carry no group at all, because they apply to the whole endpoint. Endpoint Discovery messages and Function Block Info Notifications are two examples.

## How Windows MIDI Services handles this

We decided early on, with hardware and software partners agreeing, to show one unified view of an endpoint, whether it's a native MIDI 2.0 endpoint or a bundle of MIDI 1.0 cables. We also decided to present every MIDI 1.0 and MIDI 2.0 message in a single format: UMP. That works because UMP was designed for it, and every MIDI 1.0 byte format message has an exact UMP equivalent.

> So Windows MIDI Services has no "ports". We do map back to ports for the older MIDI 1.0 APIs, but only so those APIs keep working.

The messages map across very neatly. The UMP specification has the details, but for a MIDI 1.0 channel voice message it looks like this:

| UMP message type | **Group** | Status | Channel index | ... |
| ----- | ----- | ----- | ----- | ----- |
| 2 | **0-15** | 8 | 2 | rest of data |

### Cables become groups

When the service enumerates a MIDI 1.0 USB device, it creates one aggregate endpoint for the whole device. As part of that it builds a map from UMP group numbers to the device's MIDI 1.0 kernel streaming pins, and uses that map to route a message to the right place when you address a group.

At the same time it creates virtual group terminal blocks for the device. The names that used to show up as MIDI 1.0 port names become the names of those group terminals.

### Message listeners, if you want something port-like

If your application really works in terms of single ports, the SDK has message listeners. Anyone can write one, and we supply listeners that watch one or more groups for exactly this case. Your app keeps one connection to the endpoint and filters messages on the client side, which scales much better than opening a connection per port.

Sending is unchanged: you send to the endpoint object, with the right group number in the message.

## How to present this to your users

In MIDI 2.0 and UMP the things you can address are the `Endpoint`, the `Group`, and the `Channel`. Make all three visible to your user somehow.

MIDI 2.0 devices usually support function blocks. These are named, they can span more than one group, and they're designed to be moved while the device is running. Prefer function blocks over group terminal blocks whenever you have them.

> The enumeration support in `MidiEndpointDeviceInformation` also supports projecting a Group Terminal Block (a USB concept) to its equivalent Function Block. So if Function Blocks are not available natively from the MIDI 2.0 device, you can still work with the same entity as projected from the Group Terminal Block.

When an endpoint reports both kinds of block, use the Function Blocks and ignore the Group Terminal Blocks. They are two descriptions of the same endpoint at different levels of authority, not two sets of ports, so merging them produces a doubled list. The precedence rule and the code to implement it are in [Porting a MIDI Library or Framework to Windows MIDI Services]({{ site.baseurl }}/kb/porting-midi-libraries/).

Group Terminal Blocks come from USB descriptors. Windows MIDI Services also synthesizes them for Bluetooth LE MIDI 1.0 devices so there is always something to enumerate, but MIDI 2.0 endpoints which are not USB, such as Network MIDI 2.0, Bluetooth LE MIDI 2.0 and virtual devices, have Function Blocks and no Group Terminal Blocks at all. They do not currently change at runtime, and are not currently renameable, although a device could create different ones the next time it is enumerated (most do not).

So one way to show this to your users would be

```
<Endpoint Name>
- Group <Group Number> (<Group Terminal Block or Function Block Name which could change>) <Channel>
- Group <Group Number> (<Group Terminal Block or Function Block Name which could change>) <Channel>
- Group <Group Number> (<Group Terminal Block or Function Block Name which could change>) <Channel>
```

For example

```
Contoso Synth
- Group 1 (Synth Engine) Channel 5
- Group 2 (Synth Engine) Channel 1
- Group 3 (Keyboard) Channel 1

```

How you present it is up to you and what your application needs. This is only one approach.

> Groups and channels are numbered 0-15 internally but displayed as 1-16. The `MidiGroup` and `MidiChannel` types handle that for you, and they also supply the abbreviations for group and channel.

This takes a little getting used to, but it's designed for what's coming. As more native MIDI 2.0 devices arrive, it will feel more natural. Most importantly, nothing you could do with ports is lost.
