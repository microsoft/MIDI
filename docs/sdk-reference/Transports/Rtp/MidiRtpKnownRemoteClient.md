---
layout: sdk_reference_page
title: MidiRtpKnownRemoteClient
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: A remote RTP-MIDI device a host has already been told to allow or deny
---

Held in [MidiRtpHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostKnownClientsConfig/).`KnownClients`, and reported in `MidiRtpConfiguredHost.KnownRemoteClients`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpKnownRemoteClient()` | Creates an empty entry, with `IsAllowed` false |
| `MidiRtpKnownRemoteClient(remoteClientName, isAllowed)` | Creates an entry for this remote device |

## Properties

| Property | Description |
| -------- | ----------- |
| `RemoteClientName` | The name the remote device sends |
| `IsAllowed` | True to let the remote device connect without asking again, and false to turn it away |

## Remarks

RTP-MIDI carries nothing else that stays the same from one connection to the next, so a remote device is recognized by the name it sends, ignoring uppercase and lowercase differences. An entry with an empty name is skipped when the configuration is saved.
