---
layout: sdk_reference_page
title: MidiBluetoothTimestampSource
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Whether a device's own timestamps are being used, or arrival time is standing in for them
---

Reported by `MidiBluetoothDeviceInformation.TimestampSource`.

Bluetooth Low Energy MIDI 1.0 sends a 13-bit millisecond timestamp with each message. The service lines it up with this PC's clock, so the timing between messages is kept even though Bluetooth sends them in bunches. Some inexpensive devices never move that timestamp forward. Every packet has the same value, no matter how much time has really passed.

Lining up with a clock that doesn't run would put a whole musical phrase at a single instant. So the service notices this, and uses the time each message arrived instead. The timing is then real, not squashed into one instant, but it's measured on this PC, so it's an estimate.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Unknown` | `0` | The device hasn't sent enough yet for the service to judge its clock |
| `Device` | `1` | The device's own timestamps are used |
| `ArrivalTime` | `2` | The device doesn't keep time, so the moment each message arrived is used instead. Timing is approximate |

The service works this out from the messages it receives. The device doesn't declare it. So it stays `Unknown` until the device has sent something, and it's checked again for as long as the device stays connected. Nothing is saved, so if a device's firmware is updated to keep time, that's noticed automatically.

An app that cares about accurate timing, such as a recorder, may want to tell people when a device reports `ArrivalTime`. The timing it records will be when messages arrived at this PC, not when they were played.
