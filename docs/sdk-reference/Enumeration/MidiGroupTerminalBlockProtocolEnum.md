---
layout: sdk_reference_page
title: MidiGroupTerminalBlockProtocol
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: The protocol information for a Group Terminal Block
---

Indicates the protocol specifics for the Group Terminal Block. Group terminal blocks are still available, but are generally deprecated by the MIDI Association in favor of function blocks, endpoint discovery and protocol negotiation, when available.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Unknown` | `0x00000000` | Unknown or undefined |
| `Midi1Message64` | `0x00000001` | Supports MIDI 1.0 messages, including 64 bit System Exclusive messages  |
| `Midi1Message64WithJitterReduction` | `0x00000002` | Supports MIDI 1.0 messages, including 64 bit System Exclusive messages * |
| `Midi1Message128` | `0x00000003` | Supports MIDI 1.0 messages, including 128 bit System Exclusive messages |
| `Midi1Message128WithJitterReduction` | `0x00000004` | Supports MIDI 1.0 messages, including 128 bit System Exclusive messages * |
| `Midi2` | `0x00000011` | Supports MIDI 2.0 messages |
| `Midi2WithJitterReduction` | `0x00000012` | Supports MIDI 2.0 messages, including 128 bit System Exclusive messages * |

\* Note. Jitter Reduction indicators in group terminal blocks should be ignored. These are now specified through endpoint discovery and protocol negotiation, and are handled completely in the MIDI service. Do not send jitter reduction messages from your application.
