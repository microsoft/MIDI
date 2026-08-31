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
| `Success` | `0x00000000` | The device was disconnected |
| `MissingBluetoothDeviceId` | `0x00000031` | No Bluetooth device id was supplied |
| `InvalidBluetoothDeviceId` | `0x00000032` | The Bluetooth device id was malformed |
| `TransportNotAvailable` | `0x00000041` | The Bluetooth MIDI transport is not running |
| `DeviceNotDiscovered` | `0x00000101` | No device with that address has been discovered |
| `NotConnected` | `0x00000111` | The device was not connected |
| `Unexpected` | `0x11002011` | An unexpected error occurred |
