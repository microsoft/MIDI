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
| `Success` | True when the connection was made, or the request was saved for when the device shows up |
| `ErrorCode` | A `MidiBluetoothDeviceConnectErrorCode` |
| `ErrorMessage` | The transport's own wording, which is more specific than the error code. Some causes share a code, and this tells them apart |
| `ErrorHResult` | The original HRESULT error number, kept so you can still figure out a cause the error code can't tell apart |
| `IsKnown` | False when this PC has never seen the address advertising. A connection request is saved either way, so this is how you know the device wasn't really there |
| `Device` | The updated `MidiBluetoothDeviceInformation`, or null when the device couldn't be found |
