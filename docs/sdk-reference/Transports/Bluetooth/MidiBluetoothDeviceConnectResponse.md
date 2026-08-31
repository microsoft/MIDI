---
layout: sdk_reference_page
title: MidiBluetoothDeviceConnectResponse
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The result of connecting a Bluetooth MIDI device
---

Returned by `MidiBluetoothTransportManager.ConnectDeviceAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True when the connection was made, or the request was remembered for when the device appears. |
| `ErrorCode` | A `MidiBluetoothDeviceConnectErrorCode`. |
| `ErrorMessage` | The transport's own wording, which is more specific than the error code can be. Some causes share a code, and this is what tells them apart. |
| `ErrorHResult` | The raw HRESULT, preserved so a cause the error code cannot distinguish is still diagnosable. |
| `IsKnown` | False when the address is not one this PC has seen advertising. A connection request is remembered either way, so this is how a caller knows the device was not actually there. |
| `Device` | The updated `MidiBluetoothDeviceInformation`, or null when the device could not be resolved. |
