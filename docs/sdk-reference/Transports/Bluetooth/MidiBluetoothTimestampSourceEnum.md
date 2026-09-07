---
layout: sdk_reference_page
title: MidiBluetoothTimestampSource
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Whether a device's own timestamps are being used, or arrival time is standing in for them
---

Reported by `MidiBluetoothDeviceInformation.TimestampSource`.

Bluetooth Low Energy MIDI 1.0 carries a thirteen bit millisecond timestamp with each message, which the service correlates against this PC's clock so that the spacing between messages survives the connection interval. Several inexpensive devices never advance that timestamp: every packet carries the same value no matter how much time has really passed.

Correlating against a clock which does not run would place an entire gesture at a single instant, so the service detects this and substitutes the time each message arrived instead. The result is honest real-time spacing rather than a collapsed instant, but it is measured at this end and is therefore an estimate.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Unknown` | `0` | The device has not sent enough for the service to judge its clock yet. |
| `Device` | `1` | The device's own timestamps are being used. |
| `ArrivalTime` | `2` | The device does not keep time, so the moment each message arrived is used instead. Timing is approximate. |

This is observed from received traffic rather than declared by the device, so it stays `Unknown` until the device has actually sent something, and it is re-evaluated for the life of the connection. Nothing is stored, so a device whose firmware is updated to keep time is picked up automatically.

An application which cares about timing accuracy, such as a recorder, may want to tell the customer when a device reports `ArrivalTime`, because the spacing it records will be this PC's view of when messages arrived rather than when they were played.
