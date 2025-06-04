---
layout: sdk_reference_page
title: MidiGroupTerminalBlockProtocol
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: The protocol information for a Group Terminal Block
---

Indicates the protocol specifics for the Group Terminal Block. Group terminal blocks are still available, but are generally deprecated by the MIDI Association in favor of function blocks, endpoint discovery and protocol negotiation, when available.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Unknown` | `0x00` | Unknown or undefined |
| `Midi1Message64` | `0x01` | Supports MIDI 1.0 messages, including 64 bit System Exclusive messages  |
| `Midi1Message64WithJitterReduction` | `0x02` | Supports MIDI 1.0 messages, including 64 bit System Exclusive messages * |
| `Midi1Message128` | `0x03` | Supports MIDI 1.0 messages, including 128 bit System Exclusive messages |
| `Midi1Message128WithJitterReduction` | `0x04` | Supports MIDI 1.0 messages, including 128 bit System Exclusive messages * |
| `Midi2` | `0x11` | Supports MIDI 2.0 messages |
| `Midi2WithJitterReduction` | `0x12` | Supports MIDI 2.0 messages, including 128 bit System Exclusive messages * |

\* Note. Jitter Reduction indicators in group terminal blocks should be ignored. These are now specified through endpoint discovery and protocol negotiation, and are handled completely in the MIDI service. Do not send jitter reduction messages from your application.
