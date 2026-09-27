---
layout: sdk_reference_page
title: MidiNetworkAdvertisedHostChangedProperties
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Which properties of an advertised network host changed
---

Reported by `MidiNetworkAdvertisedHostUpdatedEventArgs.ChangedProperties`. This is a flags enumeration, so more than one value can be set at once. Test for each one with a bitwise AND, not with an equals comparison.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `None` | `0x00000000` | Nothing tracked here changed |
| `HostName` | `0x00000001` | The host name changed |
| `Port` | `0x00000002` | The port changed |
| `IPv4Addresses` | `0x00000004` | The list of IPv4 addresses changed |
| `IPv6Addresses` | `0x00000008` | The list of IPv6 addresses changed |
| `TextAttributes` | `0x00000010` | The mDNS TXT attributes changed |

Your handler can use this to skip updates it doesn't care about, instead of reading everything again. Addresses change often on a PC with several network adapters or with IPv6 privacy addresses turned on. A list that rebuilds itself on every one of those updates will flicker for no reason.
