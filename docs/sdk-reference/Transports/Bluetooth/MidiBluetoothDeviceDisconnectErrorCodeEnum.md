---
layout: sdk_reference_page
title: MidiBluetoothDeviceDisconnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Error codes returned when disconnecting a Bluetooth MIDI device
---

Returned in `MidiBluetoothDeviceDisconnectResponse`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Success` | `0` | The device was disconnected |
| `DeviceNotFound` | `1` | No device with that address has been discovered |
| `NotConnected` | `2` | The device was not connected |
| `InvalidBluetoothDeviceId` | `7` | The Bluetooth device id was missing or malformed |
| `TransportNotAvailable` | `8` | The Bluetooth MIDI transport is not running |
| `DisconnectFailed` | `9` | The device could not be disconnected |
| `Unexpected` | `2000` | An unexpected error occurred |
