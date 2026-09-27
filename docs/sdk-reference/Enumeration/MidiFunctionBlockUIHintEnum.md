---
layout: sdk_reference_page
title: MidiFunctionBlockUIHint
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: Specifies the primary use of a function block
---

MIDI 2.0 sends messages both ways, but to the person using it, a function block may be mostly an input or mostly an output. The UI hint, whose values come from the UMP specification, suggests how to show the block to people. For example, a tone generator might send messages so it can say which sound is playing, but to the person using it, it's mainly something that receives notes.

Don't use these values to stop people from doing things with a function block's groups. Use them to decide how to show the information. For example, you might show only the receiving functions and groups at first, with a "Show all" option for the rest.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Unknown` | `0x00000000` | Not known or not set. |
| `Receiver` | `0x00000001` | This block mostly receives MIDI data. For example, a tone generator. |
| `Sender` | `0x00000002` | This block mostly sends MIDI data. For example, a keyboard or a grid of touch pads. |
| `Bidirectional` | `0x00000003` | This block sends and receives. For example, a sequencer that can both record and play notes. |
