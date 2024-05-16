---
layout: page
title: Mapping MIDI 1.0 Ports
has_children: false
---

# Mapping MIDI 1.0 Ports to UMP Endpoints

The big conceptual change between MIDI 1.0 and MIDI 2.0 / UMP (Universal MIDI Packet) endpoints is that the concept of a "Port" is no longer used.

## Background

In USB MIDI 1.0, messages are sent over USB using a 32-bit packet. That packet includes a virtual cable identifier, which is a number which maps to one of 16 possible virtual cables on the endpoint. This allows a single MIDI 1.0 device to have, for example, 8 input ports and 8 output ports on the single endpoint.

MIDI 2.0 UMP has a similar concept, but the logical equivalent of that cable is a group. Because the group index is included in the message itself, and not in additional data sent to the device, all addressing required to route within an endpoint is included in the message.

Additionally, some MIDI 2.0 UMP messages have no group information because they apply to the entire endpoint. For example, Endpoint Discovery messages, Function Block Info Notifications, and others.

## How we approach this in Windows MIDI Services

With Windows MIDI Services, we made the decision early on, validated by hardware and software partners, to provide a single unified view of an endpoint, whether it represents a MIDI 2.0 native endpoint or a collection of MIDI 1.0 cables on an endpoint. We also decided, because UMP was designed with this in mind and includes 1:1 mappings between MIDI 1.0 byte-format messages and their MIDI 1.0 Protocol in UMP equivalents, to present all MIDI 1.0 and MIDI 2.0 messages using a single format: UMP.

> That means that Windows MIDI Services has no "ports". (We do map back to ports for our older MIDI 1.0 APIs, but that is for backwards compatibility with those APIs)

The way the messages themselves map over is super clean. It's documented in the MIDI 2.0 UMP spec, but in a nut shell, it looks like this (in the case of a MIDI 1.0 Channel Voice message):

| UMP Message Type | **Group** | Status | Channel Index | Channel and Status | ... |
| 2 | **0-15** | 8 | 2 | rest of data |

### Cables to Groups

When we enumerate a MIDI 1.0 USB device in the service, we create an aggregate endpoint for that device. As part of that, we build a map of UMP group indexes to MIDI 1.0 KS Pins. We use this for routing so when you address a specific Group.

At the same time, we create virtual Group Terminal Blocks for that device. The names that normally show up as the port name in MIDI 1.0, now become the name of the group terminal.

### Message listeners for MIDI 1.0 port-like access

If your application is set up to deal more with single ports, we have an affordance in the SDK: the Message Listener. Anyone can build a message listener, but we supply ones to listen to one or more groups specifically for this use-case. This enables us to scale better by keeping only one connection to the endpoint, but filtering the messages on the client.

Sending messages are still accomplished by communicating directly with the endpoint object, with an appropriate Group index in the message itself.

## How to present this to your users

In MIDI 2.0 and UMP, the addressible entities are the Endpoint, the Group, and the Channel. That information should always be made available in some way to your user.

MIDI 2.0 devices typically support Function Blocks, which are named entities which can span one or more groups, and which can be moved at runtime, by design. Whenever possible, Function Blocks are preferred over Group Terminal Blocks.

> The enumeration support in `MidiEndpointDeviceInformation` also supports projecting a Group Terminal Block (a USB concept) to its equivalent Function Block. So if Function Blocks are not available natively from the MIDI 2.0 device, you can still work with the same entity as projected from the Group Terminal Block.

Group Terminal Blocks are only for USB devices, but they are also static. They do not change at runtime, but a device could potentially create different ones the next time it is created (most do not).

So when presenting the information to your users, one possible format would be

```
<Endpoint Name>
- Group <Group Number> (<Group Terminal Block or Function Block Name which could change>) <Channel>
- Group <Group Number> (<Group Terminal Block or Function Block Name which could change>) <Channel>
- Group <Group Number> (<Group Terminal Block or Function Block Name which could change>) <Channel>
```

An example would be

```
Contoso Synth
- Group 1 (Synth Engine) Channel 5
- Group 2 (Synth Engine) Channel 1
- Group 3 (Keyboard) Channel 1

```

Of course, how you present this to your users is up to you and the unique needs of your application. This is only one possible approach.

> Note that Groups, like Channels, are 0-15 for the index, but 1-16 for the display value. The `MidiGroup` and `MidiChannel` types handle this for you automatically, if you use them. They also include the abbrviations for Group and Channel.

It may take a little getting used to to work with the new endpoint approach, but this is designed for the future, so as more MIDI 2.0 native devices arrive on market, the approach will seem more natural. And most importantly, no functionality is lost compared to the old approach.
