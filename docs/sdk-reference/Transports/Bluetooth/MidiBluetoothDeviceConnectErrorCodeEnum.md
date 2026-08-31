---
layout: sdk_reference_page
title: MidiBluetoothDeviceConnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Error codes returned when connecting a Bluetooth MIDI device
---

Returned in `MidiBluetoothDeviceConnectResponse`.

Causes the transport cannot tell apart share a value here rather than being guessed at. When that happens, `ErrorMessage` carries the transport's own wording, which is more specific, and `ErrorHResult` carries the raw value.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Success` | `0` | The device connected, or the request was remembered for when it appears |
| `DeviceNotFound` | `1` | No usable MIDI endpoint at that address. The device may never have been discovered, may not expose the Bluetooth MIDI service, or may expose it without a MIDI characteristic. |
| `DeviceNotAvailable` | `2` | Discovered, but Windows could not open it. Usually asleep, out of range, or already connected to another host. |
| `DeviceInUse` | `3` | Something else already holds this device's MIDI service open. The in-box Windows Bluetooth MIDI 1.0 support claims paired devices and holds them exclusively. |
| `AlreadyConnected` | `4` | The device is already connected |
| `AccessDenied` | `5` | Windows denied access to the device's GATT services |
| `OperationAborted` | `6` | The transport was shutting down, or the operation was canceled part way through |
| `InvalidBluetoothDeviceId` | `7` | The Bluetooth device id was missing or malformed |
| `TransportNotAvailable` | `8` | The Bluetooth MIDI transport is not running |
| `ConnectFailed` | `9` | The device's MIDI service or session could not be opened |
| `Unexpected` | `2000` | An unexpected error occurred |
