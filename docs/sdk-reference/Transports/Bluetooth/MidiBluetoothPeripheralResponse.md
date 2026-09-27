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
| `Success` | True when it worked |
| `ErrorCode` | A `MidiBluetoothPeripheralErrorCode` |
| `ErrorMessage` | The transport's own wording for the failure |
| `ErrorHResult` | The original HRESULT error number |
| `Status` | The `MidiBluetoothPeripheralStatus` afterward, whether or not it worked |
