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
| `Success` | True when the device was disconnected. |
| `ErrorCode` | A `MidiBluetoothDeviceDisconnectErrorCode`. |
| `ErrorMessage` | The transport's own wording for the failure. |
| `ErrorHResult` | The raw HRESULT. |

`NotConnected` and `DeviceNotFound` are not necessarily failures worth stopping for. Removing a device from the configuration file is still meaningful when it is not connected, which is exactly when a customer is most likely to want it.
