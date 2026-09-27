---
layout: sdk_reference_page
title: MidiBluetoothDeviceDisconnectResponse
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The result of disconnecting a Bluetooth MIDI device
---

Returned by `MidiBluetoothTransportManager.DisconnectDeviceAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True when the device was disconnected |
| `ErrorCode` | A `MidiBluetoothDeviceDisconnectErrorCode` |
| `ErrorMessage` | The transport's own wording for the failure |
| `ErrorHResult` | The original HRESULT error number |

`NotConnected` and `DeviceNotDiscovered` aren't always failures worth stopping for. Removing a device from the configuration still makes sense when it isn't connected, and that's exactly when people are most likely to want to do it.
