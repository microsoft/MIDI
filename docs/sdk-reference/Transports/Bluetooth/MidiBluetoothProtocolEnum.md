---
layout: sdk_reference_page
title: MidiBluetoothProtocol
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Which Bluetooth MIDI protocol a device speaks
---

Which Bluetooth Low Energy MIDI protocol is in use. The service reports it. You don't choose it. When a device offers both, the service always picks Bluetooth Low Energy MIDI 2.0.

The one place an app does choose is `MidiBluetoothPeripheralConfig`, because a peripheral has to advertise one protocol or the other.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Unknown` | `0` | Not known yet. A device doesn't report its protocol until it's connected |
| `BluetoothLowEnergyMidi1` | `1` | Bluetooth Low Energy MIDI 1.0, the widely used Apple-compatible transport |
| `BluetoothLowEnergyMidi2Ump` | `2` | Bluetooth Low Energy MIDI 2.0, carrying UMP. It's a draft standard, and almost nothing uses it yet |
