---
layout: sdk_reference_page
title: MidiBluetoothOfflineRetention
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: How long a device's MIDI endpoint outlives the device going offline
---

Bluetooth MIDI devices come and go constantly. They sleep to save power, they go out of range, and their batteries die. This controls what happens to the MIDI endpoint when that happens.

Any value greater than zero is a number of seconds, so this enumeration only names the values that aren't an amount of time. Because of that, the API takes and returns a plain `Int32`, not this type. Use these named values for the special cases, and a number greater than zero for a delay.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `UseTransportDefault` | `-2` | Only for a single device. Uses the transport-wide setting |
| `KeepAlways` | `-1` | The endpoint stays until someone disconnects the device. This is the default |
| `Immediate` | `0` | The endpoint and its MIDI 1.0 ports are removed as soon as the connection drops, and created again when the device comes back |

## Why this is a setting

Neither choice is right for every app, so it's up to the person using the PC.

Keeping the endpoint is easier. Apps keep their MIDI ports, and when the device comes back, it just starts working again, with nothing to reopen or pick again.

Removing it matters because an app written for WinMM or WinRT MIDI 1.0 can't ask Windows whether a device is present. Whether the port still exists is the only clue it gets. For those apps, a device that has quietly gone away while its port stays open is worse than one whose port disappears, because messages sent to it are just lost.

Set it for one device or the whole transport with `MidiBluetoothOfflineRetentionConfig`, and read the current values from `MidiBluetoothDeviceInformation.OfflineRetentionSeconds` and `EffectiveOfflineRetentionSeconds`.
