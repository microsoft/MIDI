---
layout: sdk_reference_page
title: MidiGroupTerminalBlockProtocol
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: The protocol information for a Group Terminal Block
---

Which protocol a group terminal block uses. Group terminal blocks still work, but the MIDI Association now recommends function blocks, endpoint discovery, and protocol negotiation instead, when a device supports them.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Unknown` | `0x00000000` | Not known or not set |
| `Midi1Message64` | `0x00000001` | MIDI 1.0 protocol, using packets up to 64 bits long |
| `Midi1Message64WithJitterReduction` | `0x00000002` | MIDI 1.0 protocol, using packets up to 64 bits long, with jitter reduction timestamps * |
| `Midi1Message128` | `0x00000003` | MIDI 1.0 protocol, using packets up to 128 bits long |
| `Midi1Message128WithJitterReduction` | `0x00000004` | MIDI 1.0 protocol, using packets up to 128 bits long, with jitter reduction timestamps * |
| `Midi2` | `0x00000011` | MIDI 2.0 protocol |
| `Midi2WithJitterReduction` | `0x00000012` | MIDI 2.0 protocol, with jitter reduction timestamps * |

\* **Note:** Ignore the jitter reduction part of these values. Jitter reduction is now agreed on through endpoint discovery and protocol negotiation, and the MIDI service handles it completely. Don't send jitter reduction messages from your application.
