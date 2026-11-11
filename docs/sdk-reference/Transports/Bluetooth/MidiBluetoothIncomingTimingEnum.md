---
layout: sdk_reference_page
title: MidiBluetoothIncomingTiming
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: When messages from a Bluetooth MIDI device are handed to apps
---

Bluetooth sends MIDI in packets, on a schedule the radio sets. So notes that were played evenly can arrive in uneven bunches. If each message is passed on the moment its packet arrives, that unevenness can be heard as timing jitter. This setting controls when incoming messages are handed to apps.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `UseTransportDefault` | `-1` | Only for a single device. Uses the transport-wide setting |
| `Smooth` | `0` | Each message is held until the time the device stamped on it, plus a small, steady delay. This adds a few milliseconds of latency, and in return the timing is much steadier. This is the default |
| `AsSoonAsPossible` | `1` | Each message is passed on as soon as its packet arrives. This has the lowest latency, but the timing is only as even as the radio link |

## Why this is a setting

Neither choice is right for everyone, so it's up to the person using the PC.

`Smooth` is best for playing and recording, where steady timing matters more than a few milliseconds. `AsSoonAsPossible` is best when the lowest possible latency matters more than steady timing.

Set it for one device or the whole transport with `MidiBluetoothIncomingTimingConfig`. Read the current values from `MidiBluetoothDeviceInformation.IncomingTiming` and `EffectiveIncomingTiming`, and see how much delay smoothing is adding right now in `IncomingSmoothingDelayMilliseconds`.