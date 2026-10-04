---
layout: sdk_reference_page
title: MidiRtpSendSpeedLimit
namespace: Windows.Devices.Midi2.Transports.Rtp
type: enum
description: How fast an RTP-MIDI host or client sends, as a multiple of MIDI 1.0 wire speed
---

Used by `SendSpeedLimit` on [MidiRtpHostCreationConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostCreationConfig/) and [MidiRtpClientConnectConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpClientConnectConfig/). Reported by [MidiRtpConfiguredHost]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConfiguredHost/), [MidiRtpConfiguredClient]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpConfiguredClient/), [MidiRtpSavedHost]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSavedHost/) and [MidiRtpSavedClient]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSavedClient/).

MIDI 1.0 wire speed is the speed of a MIDI 1.0 DIN cable: 31,250 bits a second, which is 3,125 bytes a second. At `Midi1WireSpeed`, a SysEx dump takes about as long as it would over a DIN cable. This is the same setting other RTP-MIDI software calls a MIDI DIN speed limit.

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

When an app sends faster than the limit, Windows holds the extra messages and sends them as fast as the limit allows. An app that keeps sending faster than that is slowed down to match, instead of having messages dropped.

A new limit applies straight away, including to connections that are already up, and doesn't disconnect anything.

RTP-MIDI can't tell this PC that a device missed data, so unlike Network MIDI 2.0 there's no option to slow down by itself. Choose the speed the device needs.
