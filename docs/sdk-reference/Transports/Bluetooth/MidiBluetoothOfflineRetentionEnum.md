---
layout: sdk_reference_page
title: MidiBluetoothOfflineRetention
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: How long a device's MIDI endpoint outlives the device going offline
---

Bluetooth MIDI devices come and go constantly. They sleep to save power, they go out of range, and their batteries die. This controls what happens to the MIDI endpoint when that happens.

Any value greater than zero is a number of seconds, so this enumeration names only the values which are not a duration. Because of that, the API takes and returns a plain `Int32` rather than this type: use these named values for the special cases and a positive number for a delay.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `UseTransportDefault` | `-2` | Only valid for a single device. Defers to the transport-wide setting. |
| `KeepAlways` | `-1` | The endpoint stays until the device is explicitly disconnected. This is the default. |
| `Immediate` | `0` | The endpoint and its MIDI 1.0 ports are removed as soon as the link drops, and recreated when the device comes back. |

## Why this is a setting rather than a fixed behavior

Neither answer is right for every application, so it is left to the customer.

Keeping the endpoint is more convenient: applications hold on to their MIDI ports, and when the device comes back it simply starts working again with nothing to reopen or reselect.

Removing it matters because an application written against WinMM or WinRT MIDI 1.0 has no way to ask Windows whether a device is present. Whether the port still exists is the only signal it gets. For those applications, a device which has silently vanished while its port stays open is worse than one whose port goes away, because messages sent to it just disappear.

Set it per device or transport-wide with `MidiBluetoothOfflineRetentionConfig`, and read the current values from `MidiBluetoothDeviceInformation.OfflineRetentionSeconds` and `EffectiveOfflineRetentionSeconds`.
