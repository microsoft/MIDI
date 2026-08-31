---
layout: sdk_reference_page
title: MidiBluetoothProtocol
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Which Bluetooth MIDI protocol a device speaks
---

Which Bluetooth Low Energy MIDI protocol is in use. This is reported rather than chosen: when a device offers both, the service always prefers Bluetooth Low Energy MIDI 2.0.

The one place an application does choose is `MidiBluetoothPeripheralConfig`, because a peripheral has to advertise as one protocol or the other.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Unknown` | `0` | Not yet known. A device does not report its protocol until it is connected. |
| `BluetoothLowEnergyMidi1` | `1` | Bluetooth Low Energy MIDI 1.0, the widely implemented Apple-compatible transport |
| `BluetoothLowEnergyMidi2Ump` | `2` | Bluetooth Low Energy MIDI 2.0 carrying UMP. This is a draft standard, and almost nothing implements it yet. |
