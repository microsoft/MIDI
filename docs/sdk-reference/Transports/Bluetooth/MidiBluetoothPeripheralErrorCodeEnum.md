---
layout: sdk_reference_page
title: MidiBluetoothPeripheralErrorCode
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Error codes returned by Bluetooth MIDI peripheral operations
---

Returned in `MidiBluetoothPeripheralResponse`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Success` | `0` | The operation succeeded |
| `AlreadyRunning` | `1` | This PC is already published |
| `NotRunning` | `2` | This PC is not published, so there was nothing to stop |
| `PeripheralNotAvailable` | `3` | The radio would not publish the GATT service. Not every Bluetooth radio or driver supports the peripheral role. |
| `NoClientConnected` | `4` | No remote device is subscribed |
| `InvalidProtocol` | `7` | A Bluetooth MIDI protocol must be chosen before this PC can be published |
| `TransportNotAvailable` | `8` | The Bluetooth MIDI transport is not running |
| `StartFailed` | `9` | The GATT service or characteristic could not be created |
| `Unexpected` | `2000` | An unexpected error occurred |
