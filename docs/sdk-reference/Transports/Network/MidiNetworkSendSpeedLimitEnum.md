---
layout: sdk_reference_page
title: MidiNetworkSendSpeedLimit
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: How fast a Network MIDI 2.0 host or client sends, as a multiple of MIDI 1.0 wire speed
---

Used by `SendSpeedLimit` on [MidiNetworkHostCreationConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostCreationConfig/), [MidiNetworkClientConnectConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientConnectConfig/), [MidiNetworkRemoteClientSettings]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkRemoteClientSettings/) and the two update configs. Reported by [MidiNetworkConfiguredHost]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkConfiguredHost/), [MidiNetworkConfiguredClient]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkConfiguredClient/) and [MidiNetworkHostConnection]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostConnection/).

MIDI 1.0 wire speed is the speed of a MIDI 1.0 DIN cable: 31,250 bits a second, which is 3,125 bytes a second. The speed is measured in the bytes the same messages would take on that cable, so at `Midi1WireSpeed` a SysEx dump takes about as long as it would over a DIN cable.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Unlimited` | `0` | No limit. Messages are sent as fast as they arrive. This is the default |
| `Midi1WireSpeed` | `1` | MIDI 1.0 wire speed |
| `Midi1WireSpeedTimes2` | `2` | Twice MIDI 1.0 wire speed |
| `Midi1WireSpeedTimes4` | `4` | 4 times MIDI 1.0 wire speed |
| `Midi1WireSpeedTimes8` | `8` | 8 times MIDI 1.0 wire speed |
| `Midi1WireSpeedTimes16` | `16` | 16 times MIDI 1.0 wire speed |
| `Midi1WireSpeedTimes32` | `32` | 32 times MIDI 1.0 wire speed, about 1 megabit a second |

## Remarks

Choose a limit for a device that loses data when a lot of it arrives at once, like a hardware synth taking a long SysEx dump, or a network to DIN bridge with a small buffer.

A limit never delays a lone message. After a quiet moment, a short burst goes out straight away: up to 64 bytes at `Midi1WireSpeed`, and 64 bytes more for each step up. Only the data after that is spaced out. So a single note or a knob turn is sent the moment it arrives, at any limit.

When an app sends faster than the limit, Windows holds the extra messages and sends them as fast as the limit allows. An app that keeps sending faster than that is slowed down to match, instead of having messages dropped. That's what spaces out a long SysEx dump sent all at once.

A new limit applies straight away, including to connections that are already up, and doesn't disconnect anything.
