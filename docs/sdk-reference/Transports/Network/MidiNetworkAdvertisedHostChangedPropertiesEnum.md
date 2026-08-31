---
layout: sdk_reference_page
title: MidiNetworkAdvertisedHostChangedProperties
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Which properties of an advertised network host changed
---

Reported by `MidiNetworkAdvertisedHostUpdatedEventArgs.ChangedProperties`. This is a flags enumeration, so test it with a bitwise and rather than comparing for equality.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `None` | `0x00000000` | Nothing tracked here changed. |
| `HostName` | `0x00000001` | The host name changed. |
| `Port` | `0x00000002` | The port changed. |
| `IPv4Addresses` | `0x00000004` | The set of IPv4 addresses changed. |
| `IPv6Addresses` | `0x00000008` | The set of IPv6 addresses changed. |
| `TextAttributes` | `0x00000010` | The mDNS TXT attributes changed. |

A handler can use this to ignore an update it does not care about rather than re-reading everything. Addresses in particular change often on a machine with several network adapters or with IPv6 privacy addresses enabled, and a list which rebuilds itself on every one of those updates will flicker for no reason.
