---
layout: sdk_reference_page
title: MidiBluetoothPeripheralResponse
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The result of starting or stopping the Bluetooth MIDI peripheral
---

Returned by `MidiBluetoothTransportManager.StartPeripheralAsync` and `MidiBluetoothTransportManager.StopPeripheralAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True when the operation succeeded. |
| `ErrorCode` | A `MidiBluetoothPeripheralErrorCode`. |
| `ErrorMessage` | The transport's own wording for the failure. |
| `ErrorHResult` | The raw HRESULT. |
| `Status` | The `MidiBluetoothPeripheralStatus` after the operation, whether or not it succeeded. |
