---
layout: api_page
title: MidiFunctionBlockUIHint
parent: Metadata
grand_parent: Midi2 core
has_children: false
---

# MidiFunctionBlockUIHint Enumeration

The MIDI 2.0 protocol is bi-directional in nature, but individual function blocks may be primarily an input or an output from the user's point of view. The UI hint enumeration (the values of which are described in the MIDI 2.0 Universal MIDI Packet specification) helps provide an indicator fro how to present the function to the user. For example, a tone generator may support bi-directional communication so it can indicate which patch is playing, but it would be primarily a receiver of MIDI note information, from the user's point of view.

In general, these values should not restrict completely what you enable a user to do with the groups in a function block, but they should help in how you present the information to the user. You may, for example, present a list of receiver functions and groups to the user initially, but provide a "show all" to show the remaining functions and groups.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Unknown` | `0x0` | Unknown or undefined. |
| `Receiver` | `0x1` | This block is primarily a receiver of MIDI data. For example, a tone generator. |
| `Sender` | `0x2` | This block is primarily a sender of MIDI data. For example, a keyboard or grid of touch pads. |
| `Bidirectional` | `0x3` | This block is bidirectional. For example, a sequencer which can both record and play notes. |

## IDL

[MidiFunctionBlockUIHint IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiFunctionBlockUIHintEnum.idl)
